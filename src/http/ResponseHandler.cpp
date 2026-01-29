#include "LLMEngine/http/ResponseHandler.hpp"

#include "LLMEngine/providers/APIClient.hpp" // Include before RequestLogger to get APIResponse definition
#include "LLMEngine/core/AnalysisResult.hpp"
#include "LLMEngine/diagnostics/DebugArtifactManager.hpp"
#include "LLMEngine/core/ErrorCodes.hpp"
#include "LLMEngine/http/HttpStatus.hpp"
#include "LLMEngine/utils/Logger.hpp" // Include before RequestLogger to get LogLevel definition
#include "LLMEngine/http/RequestLogger.hpp"
#include "LLMEngine/http/ResponseParser.hpp"

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
            return api_error; // Use the specific error code
        case LLMEngineErrorCode::None:
            // Fall through to status code-based classification
            break;
        case LLMEngineErrorCode::Unknown:
        default:
            // Classify based on status code if error code is Unknown
            if (status_code == HttpStatus::TOO_MANY_REQUESTS) {
                return LLMEngineErrorCode::RateLimited;
            } else if (HttpStatus::isClientError(status_code)) {
                return LLMEngineErrorCode::Client;
            } else if (HttpStatus::isServerError(status_code)) {
                return LLMEngineErrorCode::Server;
            }
            return LLMEngineErrorCode::Unknown;
    }

    // If error code is None but success is false, classify by status code
    if (status_code == HttpStatus::TOO_MANY_REQUESTS) {
        return LLMEngineErrorCode::RateLimited;
    }
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
    // Write API response artifact with exception safety
    // If artifact writing fails, log the error but continue processing
    if (debug_mgr) {
        try {
            debug_mgr->writeApiResponse(api_response, !api_response.success);
            // Trigger cleanup after writing API response to enforce retention consistently
            // This ensures cleanup runs even when requests fail before think/content artifacts are
            // produced
            debug_mgr->performCleanup();
        } catch (const std::exception& e) {
            RequestLogger::logSafe(
                logger,
                LogLevel::Warn,
                std::string("Failed to write API response artifact: ") + e.what());
        } catch (...) {
            RequestLogger::logSafe(
                logger, LogLevel::Warn, "Failed to write API response artifact: unknown error");
        }
    }

    // Error path
    if (!api_response.success) {
        const std::string redacted_err = RequestLogger::redactText(api_response.errorMessage);

        // Build enhanced error message with context
        std::string enhanced_error = redacted_err;
        if (api_response.statusCode > 0) {
            enhanced_error =
                "HTTP " + std::to_string(api_response.statusCode) + ": " + enhanced_error;
        }

        // Use safe logging to ensure secrets are redacted (though enhanced_error is already
        // redacted)
        RequestLogger::logSafe(
            logger, LogLevel::Error, std::string("API error: ") + enhanced_error);
        if (logger && write_debug_files && debug_mgr) {
            logger->log(LogLevel::Info,
                        std::string("Error response saved to ") + request_tmp_dir
                            + "/api_response_error.json");
        }
        // Classify error code (now using unified LLMEngineErrorCode)
        LLMEngineErrorCode error_code =
            classifyErrorCode(api_response.errorCode, api_response.statusCode);
        // Return enhanced error message with context
        AnalysisResult result{.success = false,
                              .think = "",
                              .content = "",
                              .finishReason = "",
                              .errorMessage = enhanced_error,
                              .statusCode = api_response.statusCode,
                              .usage = api_response.usage,
                              .logprobs = std::nullopt,
                              .errorCode = error_code,
                              .tool_calls = {}};
        return result;
    }

    // Success path - write full response with exception safety
    const std::string& full_response = api_response.content;
    if (debug_mgr) {
        try {
            debug_mgr->writeFullResponse(full_response);
        } catch (const std::exception& e) {
            RequestLogger::logSafe(
                logger,
                LogLevel::Warn,
                std::string("Failed to write full response artifact: ") + e.what());
        } catch (...) {
            RequestLogger::logSafe(
                logger, LogLevel::Warn, "Failed to write full response artifact: unknown error");
        }
    }

    const auto [think_section, remaining_section] = ResponseParser::parseResponse(full_response);
    AnalysisResult result{.success = true,
                          .think = think_section,
                          .content = remaining_section,
                          .finishReason = api_response.finishReason,
                          .errorMessage = "",
                          .statusCode = api_response.statusCode,
                          .usage = api_response.usage,
                          .logprobs = std::nullopt,
                          .errorCode = LLMEngineErrorCode::None,
                          .tool_calls = {}};

    // Extract tool calls if present in raw_response
    // Logic: Look for "choices"[0]["message"]["tool_calls"]
    // Note: This relies on provider implementations populating raw_response with standard structure
    // or specific logic. Currently OpenAICompatibleClient does this.
    try {
        const auto& raw = api_response.rawResponse;
        if (raw.contains("choices") && raw["choices"].is_array() && !raw["choices"].empty()) {
            const auto& choice = raw["choices"][0];
            if (choice.contains("message") && choice["message"].contains("tool_calls")) {
                const auto& tools = choice["message"]["tool_calls"];
                if (tools.is_array()) {
                    for (const auto& tool : tools) {
                        AnalysisResult::ToolCall tc;
                        if (tool.contains("id"))
                            tc.id = tool["id"].get<std::string>();
                        if (tool.contains("function")) {
                            const auto& fn = tool["function"];
                            if (fn.contains("name"))
                                tc.name = fn["name"].get<std::string>();
                            if (fn.contains("arguments"))
                                tc.arguments = fn["arguments"].get<std::string>();
                        }
                        result.tool_calls.push_back(tc);
                    }
                }
            }
        }
    } catch (...) {
        // Ignore parsing errors for optional tool calls
        RequestLogger::logSafe(
            logger, LogLevel::Warn, "Failed to parse tool calls from raw response");
    }

    return result;
}

} // namespace LLMEngine
