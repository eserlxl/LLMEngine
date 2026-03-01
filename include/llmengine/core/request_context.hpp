#pragma once
#include "llmengine/diagnostics/debug_artifact_manager.hpp"
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

namespace LLMEngine {

struct RequestContext {
    std::string requestTmpDir;
    std::string fullPrompt;
    nlohmann::json finalParams;
    std::unique_ptr<DebugArtifactManager> debugManager;
    bool writeDebugFiles{false};
    std::string analysisType; // preserved for routing/telemetry
};

} // namespace LLMEngine
