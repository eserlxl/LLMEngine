#include "LLMEngine/RequestContextBuilder.hpp"
#include "LLMEngine/LLMEngine.hpp"
#include "LLMEngine/PromptBuilder.hpp"
#include "LLMEngine/DebugArtifactManager.hpp"
#include "LLMEngine/ParameterMerger.hpp"
#include <sstream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <random>
#include <atomic>

namespace LLMEngine {

namespace {
    // Thread-safe counter for request uniqueness within the same millisecond
    std::atomic<uint64_t> request_counter{0};
    
    // Thread-local random number generator for additional uniqueness
    thread_local std::mt19937_64 rng(std::chrono::steady_clock::now().time_since_epoch().count());
    
    /**
     * @brief Generate a unique request directory name with collision avoidance.
     * 
     * Format: req_{milliseconds}_{thread_hash}_{counter}_{random}
     * 
     * This ensures uniqueness even when multiple requests occur in the same
     * millisecond on the same thread by adding a counter and random component.
     */
    std::string generateUniqueRequestDirName(const std::filesystem::path& base) {
        const auto now = std::chrono::system_clock::now();
        const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        const auto thread_id = std::this_thread::get_id();
        std::hash<std::thread::id> hasher;
        const uint64_t thread_hash = hasher(thread_id);
        const uint64_t counter = request_counter.fetch_add(1, std::memory_order_relaxed);
        const uint64_t random_component = rng();
        
        std::ostringstream ss;
        ss << "req_" << millis << "_" << std::hex << thread_hash 
           << "_" << std::dec << counter << "_" << std::hex << random_component;
        
        std::filesystem::path request_dir = base / ss.str();
        
        // Check for collisions and append additional random component if needed
        std::error_code ec;
        int attempts = 0;
        constexpr int max_attempts = 10;
        while (std::filesystem::exists(request_dir, ec) && attempts < max_attempts) {
            ss.str("");
            ss.clear();
            ss << "req_" << millis << "_" << std::hex << thread_hash 
               << "_" << std::dec << counter << "_" << std::hex << random_component 
               << "_" << rng();
            request_dir = base / ss.str();
            ++attempts;
        }
        
        return request_dir.string();
    }
}

RequestContext RequestContextBuilder::build(const LLMEngine& engine,
                                            std::string_view prompt,
                                            const nlohmann::json& input,
                                            std::string_view analysis_type,
                                            std::string_view mode,
                                            bool prepend_terse_instruction) {
    RequestContext ctx;
    ctx.analysisType = std::string(analysis_type);

    // Generate unique request directory name with collision avoidance
    std::filesystem::path base = std::filesystem::path(engine.getTempDirectory()).lexically_normal();
    ctx.requestTmpDir = generateUniqueRequestDirName(base);

    // Prompt construction using engine's builders
    std::string full_prompt;
    if (prepend_terse_instruction && engine.getTersePromptBuilder()) {
        full_prompt = engine.getTersePromptBuilder()->buildPrompt(prompt);
    } else if (engine.getPassthroughPromptBuilder()) {
        full_prompt = engine.getPassthroughPromptBuilder()->buildPrompt(prompt);
    } else {
        PassthroughPromptBuilder fallback;
        full_prompt = fallback.buildPrompt(prompt);
    }
    ctx.fullPrompt = std::move(full_prompt);

    // Parameters merge without an unnecessary initial copy
    nlohmann::json merged_params;
    if (ParameterMerger::mergeInto(engine.getModelParams(), input, mode, merged_params)) {
        ctx.finalParams = std::move(merged_params);
    } else {
        ctx.finalParams = engine.getModelParams();
    }

    // Determine debug artifacts writing and create manager if needed
    ctx.writeDebugFiles = engine.areDebugFilesEnabled();
    if (ctx.writeDebugFiles && engine.getArtifactSink()) {
        engine.prepareTempDirectory();
        auto loggerPtr = engine.getLogger() ? engine.getLogger().get() : nullptr;
        ctx.debugManager = engine.getArtifactSink()->create(ctx.requestTmpDir, engine.getTempDirectory(), engine.getLogRetentionHours(), loggerPtr);
        if (ctx.debugManager) {
            ctx.debugManager->ensureRequestDirectory();
        }
    }

    return ctx;
}

} // namespace LLMEngine
