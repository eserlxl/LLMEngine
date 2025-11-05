#include "LLMEngine/RequestContextBuilder.hpp"
#include "LLMEngine/LLMEngine.hpp"
#include "LLMEngine/PromptBuilder.hpp"
#include "LLMEngine/DebugArtifactManager.hpp"
#include "LLMEngine/ParameterMerger.hpp"
#include <sstream>
#include <filesystem>
#include <chrono>
#include <thread>

namespace LLMEngine {

RequestContext RequestContextBuilder::build(const LLMEngine& engine,
                                            std::string_view prompt,
                                            const nlohmann::json& input,
                                            std::string_view analysis_type,
                                            std::string_view mode,
                                            bool prepend_terse_instruction) {
    RequestContext ctx;
    ctx.analysisType = std::string(analysis_type);

    // Unique request directory name
    const auto now = std::chrono::system_clock::now();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    const auto thread_id = std::this_thread::get_id();
    std::hash<std::thread::id> hasher;
    const uint64_t thread_hash = hasher(thread_id);

    std::filesystem::path base = std::filesystem::path(engine.getTempDirectory()).lexically_normal();
    std::filesystem::path request_dir = base / (std::string("req_") + std::to_string(millis) + "_");
    std::stringstream ss; ss << std::hex << thread_hash; request_dir += ss.str();
    ctx.requestTmpDir = request_dir.string();

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
