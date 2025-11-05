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
    if (prepend_terse_instruction && engine.terse_prompt_builder_) {
        full_prompt = engine.terse_prompt_builder_->buildPrompt(prompt);
    } else if (engine.passthrough_prompt_builder_) {
        full_prompt = engine.passthrough_prompt_builder_->buildPrompt(prompt);
    } else {
        PassthroughPromptBuilder fallback;
        full_prompt = fallback.buildPrompt(prompt);
    }
    ctx.fullPrompt = std::move(full_prompt);

    // Parameters merge
    nlohmann::json merged_params;
    ctx.finalParams = engine.model_params_;
    if (ParameterMerger::mergeInto(engine.model_params_, input, mode, merged_params)) {
        ctx.finalParams = std::move(merged_params);
    }

    // Determine debug artifacts writing and create manager if needed
    ctx.writeDebugFiles = engine.debug_ && !engine.disable_debug_files_env_cached_;
    if (ctx.writeDebugFiles && engine.artifact_sink_) {
        engine.ensureSecureTmpDir();
        ctx.debugManager = engine.artifact_sink_->create(ctx.requestTmpDir, engine.tmp_dir_, engine.log_retention_hours_, engine.logger_ ? engine.logger_.get() : nullptr);
        if (ctx.debugManager) {
            ctx.debugManager->ensureRequestDirectory();
        }
    }

    return ctx;
}

} // namespace LLMEngine
