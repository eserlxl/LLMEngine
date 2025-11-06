#include "LLMEngine/ResponseHandler.hpp"
#include "LLMEngine/DebugArtifactManager.hpp"
#include "LLMEngine/RequestLogger.hpp"
#include "LLMEngine/ResponseParser.hpp"
#include "LLMEngine/LLMEngine.hpp"
#include "LLMEngine/APIClient.hpp"
#include "LLMEngine/HttpStatus.hpp"

namespace LLMEngine {

// Map APIResponse::APIError to AnalysisErrorCode
static AnalysisErrorCode mapErrorCode(::LLMEngineAPI::APIResponse::APIError api_error, int status_code) {
    switch (api_error) {
        case ::LLMEngineAPI::APIResponse::APIError::Network:
            return AnalysisErrorCode::Network;
        case ::LLMEngineAPI::APIResponse::APIError::Timeout:
            return AnalysisErrorCode::Timeout;
        case ::LLMEngineAPI::APIResponse::APIError::InvalidResponse:
            return AnalysisErrorCode::InvalidResponse;
        case ::LLMEngineAPI::APIResponse::APIError::Auth:
            return AnalysisErrorCode::Auth;
        case ::LLMEngineAPI::APIResponse::APIError::RateLimited:
            return AnalysisErrorCode::RateLimited;
        case ::LLMEngineAPI::APIResponse::APIError::Server:
            return AnalysisErrorCode::Server;
        case ::LLMEngineAPI::APIResponse::APIError::None:
            // Fall through to status code-based classification
            break;
        case ::LLMEngineAPI::APIResponse::APIError::Unknown:
        default:
            // Classify based on status code if error code is Unknown
            if (HttpStatus::isClientError(status_code)) {
                return AnalysisErrorCode::Client;
            } else if (HttpStatus::isServerError(status_code)) {
                return AnalysisErrorCode::Server;
            }
            return AnalysisErrorCode::Unknown;
    }
    
    // If APIError::None but success is false, classify by status code
    if (HttpStatus::isClientError(status_code)) {
        if (status_code == HttpStatus::UNAUTHORIZED || status_code == HttpStatus::FORBIDDEN) {
            return AnalysisErrorCode::Auth;
        }
        return AnalysisErrorCode::Client;
    }
    if (HttpStatus::isServerError(status_code)) {
        return AnalysisErrorCode::Server;
    }
    
    return AnalysisErrorCode::Unknown;
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
        // Map API error code to AnalysisErrorCode
        AnalysisErrorCode error_code = mapErrorCode(api_response.error_code, api_response.status_code);
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
    result.errorCode = AnalysisErrorCode::None;
    return result;
}

} // namespace LLMEngine
