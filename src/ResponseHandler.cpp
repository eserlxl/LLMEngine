#include "LLMEngine/ResponseHandler.hpp"
#include "LLMEngine/DebugArtifactManager.hpp"
#include "LLMEngine/RequestLogger.hpp"
#include "LLMEngine/ResponseParser.hpp"
#include "LLMEngine/LLMEngine.hpp"
#include "LLMEngine/APIClient.hpp"

namespace LLMEngine {

AnalysisResult ResponseHandler::handle(const LLMEngineAPI::APIResponse& api_response,
                                       DebugArtifactManager* debug_mgr,
                                       const std::string& request_tmp_dir,
                                       std::string_view analysis_type,
                                       bool write_debug_files,
                                       void* logger_ptr) {
    Logger* logger = static_cast<Logger*>(logger_ptr);

    // Write API response artifact
    if (debug_mgr) {
        debug_mgr->writeApiResponse(nullptr, api_response, !api_response.success);
    }

    // Error path
    if (!api_response.success) {
        if (logger) {
            const std::string redacted_err = RequestLogger::redactText(api_response.error_message);
            logger->log(LogLevel::Error, std::string("API error: ") + redacted_err);
            if (write_debug_files && debug_mgr) {
                logger->log(LogLevel::Info, std::string("Error response saved to ") + request_tmp_dir + "/api_response_error.json");
            }
        }
        return AnalysisResult{false, "", "", api_response.error_message, api_response.status_code};
    }

    // Success
    const std::string& full_response = api_response.content;
    if (debug_mgr) { debug_mgr->writeFullResponse(full_response); }
    const auto [think_section, remaining_section] = ResponseParser::parseResponse(full_response);
    if (debug_mgr) { debug_mgr->writeAnalysisArtifacts(analysis_type, think_section, remaining_section); }
    return AnalysisResult{true, think_section, remaining_section, "", api_response.status_code};
}

} // namespace LLMEngine
