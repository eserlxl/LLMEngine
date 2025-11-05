#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include <memory>

namespace LLMEngine {

class DebugArtifactManager;
class LLMEngine;
class Logger;

struct RequestContext {
    std::string requestTmpDir;
    std::string fullPrompt;
    nlohmann::json finalParams;
    std::unique_ptr<DebugArtifactManager> debugManager;
    bool writeDebugFiles { false };
};

class RequestContextBuilder {
public:
    static RequestContext build(const LLMEngine& engine,
                                std::string_view prompt,
                                const nlohmann::json& input,
                                std::string_view analysis_type,
                                std::string_view mode,
                                bool prepend_terse_instruction);
};

} // namespace LLMEngine
