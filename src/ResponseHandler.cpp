#include "LLMEngine/ResponseHandler.hpp"
#include "LLMEngine/DebugArtifactManager.hpp"
#include "LLMEngine/RequestLogger.hpp"
#include "LLMEngine/ResponseParser.hpp"
#include "LLMEngine/LLMEngine.hpp"
#include "LLMEngine/APIClient.hpp"
#include "LLMEngine/ErrorCodes.hpp"
#include "LLMEngine/HttpStatus.hpp"

namespace LLMEngine {

// Map unified error code (now both use LLMEngineErrorCode, but handle status code classification)
static LLMEngineErrorCode classifyErrorCode(LLMEngineErrorCode api_error, int status_code) {
    // If we already have a specific error code, use it (unless it's None or Unknown)
    switch (api_error) {
        case LLMEngineErrorCode::Network:
        case LLMEngineErrorCode::Timeout:
        case LLMEngineErrorCode::InvalidResponse:
        case LLMEngineErrorCode::Auth:
        case LLMEngineErrorCode::RateLimited:
        case LLMEngineErrorCode::Server:
        case LLMEngineErrorCode::Client:
            return api_error;  // Use the specific error code
        case LLMEngineErrorCode::None:
            // Fall through to status code-based classification
            break;
        case LLMEngineErrorCode::Unknown:
        default:
            // Classify based on status code if error code is Unknown
            if (HttpStatus::isClientError(status_code)) {
                return LLMEngineErrorCode::Client;
            } else if (HttpStatus::isServerError(status_code)) {
                return LLMEngineErrorCode::Server;
            }
            return LLMEngineErrorCode::Unknown;
    }
    
    // If error code is None but success is false, classify by status code
    if (HttpStatus::isClientError(status_code)) {
        if (status_code == HttpStatus::UNAUTHORIZED || status_code == HttpStatus::FORBIDDEN) {
            return LLMEngineErrorCode::Auth;
        }
        return LLMEngineErrorCode::Client;
    }
    if (HttpStatus::isServerError(status_code)) {
        return LLMEngineErrorCode::Server;
    }
    
    return LLMEngineErrorCode::Unknown;
}

AnalysisResult ResponseHandler::handle(const LLMEngineAPI::APIResponse& api_response,
                                       DebugArtifactManager* debug_mgr,
                                       const std::string& request_tmp_dir,
                                       std::string_view /*analysis_type*/,
                                       bool write_debug_files,
                                       Logger* logger) {

    // Write API response artifact
    if (debug_mgr) {
        debug_mgr->writeApiResponse(api_response, !api_response.success);
    }

    // Error path
    if (!api_response.success) {
        const std::string redacted_err = RequestLogger::redactText(api_response.error_message);
        if (logger) {
            logger->log(LogLevel::Error, std::string("API error: ") + redacted_err);
            if (write_debug_files && debug_mgr) {
                logger->log(LogLevel::Info, std::string("Error response saved to ") + request_tmp_dir + "/api_response_error.json");
            }
        }
        // Classify error code (now using unified LLMEngineErrorCode)
        LLMEngineErrorCode error_code = classifyErrorCode(api_response.error_code, api_response.status_code);
        // Return redacted error back to callers to avoid leaking secrets
        AnalysisResult result{false, "", "", redacted_err, api_response.status_code};
        result.errorCode = error_code;
        return result;
    }

    // Success
    const std::string& full_response = api_response.content;
    if (debug_mgr) { debug_mgr->writeFullResponse(full_response); }
    const auto [think_section, remaining_section] = ResponseParser::parseResponse(full_response);
    AnalysisResult result{true, think_section, remaining_section, "", api_response.status_code};
    result.errorCode = LLMEngineErrorCode::None;
    return result;
}

} // namespace LLMEngine
