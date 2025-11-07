#pragma once
#include <memory>
#include <string>

namespace LLMEngine {

class DebugArtifactManager;
struct Logger;
struct AnalysisResult; // Forward declaration - full definition in AnalysisResult.hpp

} // namespace LLMEngine

namespace LLMEngineAPI {
struct APIResponse;
}

namespace LLMEngine {

class ResponseHandler {
  public:
    static AnalysisResult handle(const LLMEngineAPI::APIResponse& api_response,
                                 DebugArtifactManager* debug_mgr,
                                 const std::string& request_tmp_dir,
                                 std::string_view analysis_type,
                                 bool write_debug_files,
                                 Logger* logger);
};

} // namespace LLMEngine
