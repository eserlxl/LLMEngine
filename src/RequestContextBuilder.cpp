#include "LLMEngine/RequestContextBuilder.hpp"
#include "LLMEngine/IModelContext.hpp"
#include "LLMEngine/PromptBuilder.hpp"
#include "LLMEngine/DebugArtifactManager.hpp"
#include "LLMEngine/ParameterMerger.hpp"
#include "LLMEngine/IArtifactSink.hpp"
#include <sstream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <random>
#include <atomic>
#include <stdexcept>

namespace LLMEngine {

namespace {
    // Thread-safe counter for request uniqueness within the same millisecond
    std::atomic<uint64_t> request_counter{0};
    
    // Thread-local random number generator for additional uniqueness
    // Seeded with a combination of time, thread ID, and random_device to ensure uniqueness
    thread_local std::mt19937_64 rng = []() {
        std::random_device rd;
        std::seed_seq seed{
            static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()),
            std::hash<std::thread::id>{}(std::this_thread::get_id()),
            static_cast<uint64_t>(rd()),
            static_cast<uint64_t>(rd())
        };
        return std::mt19937_64(seed);
    }();
    
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

RequestContext RequestContextBuilder::build(const IModelContext& context,
                                            std::string_view prompt,
                                            const nlohmann::json& input,
                                            std::string_view analysis_type,
                                            std::string_view mode,
                                            bool prepend_terse_instruction) {
    RequestContext ctx;
    ctx.analysisType = std::string(analysis_type);

    // Validate temp directory path before using it
    const std::string temp_dir = context.getTempDirectory();
    if (temp_dir.empty()) {
        throw std::runtime_error("Temporary directory path is empty");
    }
    
    // Generate unique request directory name with collision avoidance
    std::filesystem::path base = std::filesystem::path(temp_dir).lexically_normal();
    if (base.empty()) {
        throw std::runtime_error("Invalid temporary directory path: " + temp_dir);
    }
    ctx.requestTmpDir = generateUniqueRequestDirName(base);

    // Prompt construction using context's builders
    std::string full_prompt;
    if (prepend_terse_instruction && context.getTersePromptBuilder()) {
        full_prompt = context.getTersePromptBuilder()->buildPrompt(prompt);
    } else if (context.getPassthroughPromptBuilder()) {
        full_prompt = context.getPassthroughPromptBuilder()->buildPrompt(prompt);
    } else {
        PassthroughPromptBuilder fallback;
        full_prompt = fallback.buildPrompt(prompt);
    }
    ctx.fullPrompt = std::move(full_prompt);

    // Parameters merge without an unnecessary initial copy
    nlohmann::json merged_params;
    if (ParameterMerger::mergeInto(context.getModelParams(), input, mode, merged_params)) {
        ctx.finalParams = std::move(merged_params);
    } else {
        ctx.finalParams = context.getModelParams();
    }

    // Determine debug artifacts writing and create manager if needed
    // Note: prepareTempDirectory() is already called in analyze() before building context,
    // but we call it here as well for safety (it's idempotent and cached)
    ctx.writeDebugFiles = context.areDebugFilesEnabled();
    if (ctx.writeDebugFiles && context.getArtifactSink()) {
        context.prepareTempDirectory();  // Ensure base directory exists (cached, idempotent)
        auto loggerPtr = context.getLogger() ? context.getLogger().get() : nullptr;
        ctx.debugManager = context.getArtifactSink()->create(ctx.requestTmpDir, context.getTempDirectory(), context.getLogRetentionHours(), loggerPtr);
        if (ctx.debugManager) {
            ctx.debugManager->ensureRequestDirectory();
        }
    }

    return ctx;
}

} // namespace LLMEngine
