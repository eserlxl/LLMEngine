// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "APIClientCommon.hpp"
#include "ChatCompletionRequestHelper.hpp"
#include "LLMEngine/APIClient.hpp"
#include "LLMEngine/Constants.hpp"
#include "LLMEngine/HttpStatus.hpp"

#include <nlohmann/json.hpp>

namespace LLMEngineAPI {

namespace {
void parseGeminiStreamChunk(std::string_view chunk, std::string& buffer, const LLMEngine::StreamCallback& callback) {
    buffer.append(chunk);

    size_t pos = 0;
    while ((pos = buffer.find("\n")) != std::string::npos) {
        std::string line = buffer.substr(0, pos);
        buffer.erase(0, pos + 1);

        if (line.empty() || line == "\r")
            continue;

        if (line.rfind("data: ", 0) == 0) {
            constexpr size_t dataPrefixLen = 6;
            std::string data = line.substr(dataPrefixLen);
            if (data == "[DONE]")
                continue;

            try {
                auto json = nlohmann::json::parse(data);
                
                if (json.contains("candidates") && json["candidates"].is_array() && !json["candidates"].empty()) {
                    const auto& cand = json["candidates"][0];
                    if (cand.contains("content") && cand["content"].contains("parts") && cand["content"]["parts"].is_array()) {
                        for (const auto& part : cand["content"]["parts"]) {
                            if (part.contains("text") && part["text"].is_string()) {
                                std::string text = part["text"].get<std::string>();
                                if (!text.empty()) {
                                    LLMEngine::StreamChunk result;
                                    result.content = text;
                                    result.is_done = false;
                                    callback(result);
                                }
                            }
                        }
                    }
                }

                if (json.contains("usageMetadata")) {
                    LLMEngine::StreamChunk result;
                    result.is_done = false;
                    result.usage = LLMEngine::AnalysisResult::UsageStats{
                        .promptTokens = json["usageMetadata"].value("promptTokenCount", 0),
                        .completionTokens = json["usageMetadata"].value("candidatesTokenCount", 0),
                        .totalTokens = json["usageMetadata"].value("totalTokenCount", 0)
                    };
                    callback(result);
                }

            } catch (const nlohmann::json::parse_error&) {
                // Ignore
            }
        }
    }
}
} // namespace

GeminiClient::GeminiClient(const std::string& api_key, const std::string& model)
    : api_key_(api_key), model_(model),
      base_url_(std::string(::LLMEngine::Constants::DefaultUrls::GEMINI_BASE)) {
    default_params_ = {{"temperature", ::LLMEngine::Constants::DefaultValues::TEMPERATURE},
                       {"max_tokens", ::LLMEngine::Constants::DefaultValues::MAX_TOKENS},
                       {"top_p", ::LLMEngine::Constants::DefaultValues::TOP_P}};
}

APIResponse GeminiClient::sendRequest(std::string_view prompt,
                                      const nlohmann::json& input,
                                      const nlohmann::json& params,
                                      const ::LLMEngine::RequestOptions& options) const {
    (void)options;
    APIResponse response;
    response.success = false;

    try {
        RetrySettings rs =
            computeRetrySettings(params, config_.get(), /*exponential_default*/ false);

        // Merge default params with provided params using update() for efficiency
        nlohmann::json request_params = default_params_;
        request_params.update(params);

        // Compose user content; prepend optional system_prompt
        std::string user_text;
        if (input.contains(std::string(::LLMEngine::Constants::JsonKeys::SYSTEM_PROMPT))
            && input[std::string(::LLMEngine::Constants::JsonKeys::SYSTEM_PROMPT)].is_string()) {
            user_text += input[std::string(::LLMEngine::Constants::JsonKeys::SYSTEM_PROMPT)]
                             .get<std::string>();
            if (!user_text.empty() && user_text.back() != '\n')
                user_text += "\n";
        }
        user_text += std::string(prompt);

        // Prepare request payload for Gemini generateContent
        nlohmann::json payload = {
            {"contents",
             nlohmann::json::array({nlohmann::json{
                 {"role", "user"},
                 {"parts", nlohmann::json::array({nlohmann::json{{"text", user_text}}})}}})},
            {"generationConfig",
             nlohmann::json{{"temperature", request_params["temperature"]},
                            {"maxOutputTokens", request_params["max_tokens"]},
                            {"topP", request_params["top_p"]}}}};

        // Get timeout from params or use config default
        int timeout_seconds = 0;
        if (params.contains(std::string(::LLMEngine::Constants::JsonKeys::TIMEOUT_SECONDS))) {
            timeout_seconds =
                params[std::string(::LLMEngine::Constants::JsonKeys::TIMEOUT_SECONDS)].get<int>();
        } else {
            timeout_seconds = config_ ? config_->getTimeoutSeconds()
                                      : APIConfigManager::getInstance().getTimeoutSeconds();
        }
        // Clamp to safe range [1, MAX_TIMEOUT_SECONDS] seconds
        timeout_seconds = std::max(timeout_seconds, 1);
        timeout_seconds =
            std::min(timeout_seconds, ::LLMEngine::Constants::DefaultValues::MAX_TIMEOUT_SECONDS);

        // SSL verification toggle (default: true for security)
        bool verify_ssl = true;
        if (params.contains("verify_ssl") && params.at("verify_ssl").is_boolean()) {
            verify_ssl = params.at("verify_ssl").get<bool>();
        }

        // SECURITY: Use header-based authentication instead of URL query parameter
        const std::string url = base_url_ + "/models/" + model_ + ":generateContent";

        std::map<std::string, std::string> hdr{{"Content-Type", "application/json"},
                                               {"x-goog-api-key", api_key_}};
        maybeLogRequest("POST", url, hdr);
        cpr::Response cpr_response = sendWithRetries(
            rs,
            [&]() {
                return cpr::Post(cpr::Url{url},
                                 cpr::Header{hdr.begin(), hdr.end()},
                                 cpr::Body{payload.dump()},
                                 cpr::Timeout{timeout_seconds * MILLISECONDS_PER_SECOND},
                                 cpr::VerifySsl{verify_ssl});
            },
            options);

        response.status_code = static_cast<int>(cpr_response.status_code);

        if (cpr_response.status_code == ::LLMEngine::HttpStatus::OK) {
            if (cpr_response.text.empty()) {
                response.error_message = "Empty response from server";
                response.error_code = LLMEngine::LLMEngineErrorCode::InvalidResponse;
            } else {
                try {
                    response.raw_response = nlohmann::json::parse(cpr_response.text);
                    if (response.raw_response.contains("candidates")
                        && response.raw_response["candidates"].is_array()
                        && !response.raw_response["candidates"].empty()) {
                        const auto& cand = response.raw_response["candidates"][0];
                        std::string aggregated;
                        if (cand.contains("content") && cand["content"].contains("parts")
                            && cand["content"]["parts"].is_array()) {
                            for (const auto& part : cand["content"]["parts"]) {
                                if (part.contains("text") && part["text"].is_string()) {
                                    aggregated += part["text"].get<std::string>();
                                }
                            }
                        }
                        if (!aggregated.empty()) {
                            response.content = aggregated;
                            response.success = true;
                        } else {
                            response.error_message = "No text content in response";
                            response.error_code = LLMEngine::LLMEngineErrorCode::InvalidResponse;
                        }
                    } else {
                        response.error_message = "Invalid response format";
                        response.error_code = LLMEngine::LLMEngineErrorCode::InvalidResponse;
                    }
                } catch (const nlohmann::json::parse_error& e) {
                    response.error_message = std::string("JSON parse error: ") + e.what();
                    response.error_code = LLMEngine::LLMEngineErrorCode::InvalidResponse;
                }
            }
        } else {
            response.error_message =
                "HTTP " + std::to_string(cpr_response.status_code) + ": " + cpr_response.text;
            if (cpr_response.status_code == ::LLMEngine::HttpStatus::UNAUTHORIZED
                || cpr_response.status_code == ::LLMEngine::HttpStatus::FORBIDDEN) {
                response.error_code = LLMEngine::LLMEngineErrorCode::Auth;
            } else if (cpr_response.status_code == ::LLMEngine::HttpStatus::TOO_MANY_REQUESTS) {
                response.error_code = LLMEngine::LLMEngineErrorCode::RateLimited;
            } else {
                response.error_code = LLMEngine::LLMEngineErrorCode::Server;
            }
            try {
                response.raw_response = nlohmann::json::parse(cpr_response.text);
            } catch (const std::exception& e) { // NOLINT(bugprone-empty-catch)
                // Best-effort JSON parsing for error responses.
                // If parsing fails, keep raw_response as default empty object.
                // This is expected behavior: error responses may not always be valid JSON.
                // The error message already contains the raw response text, so no information is
                // lost. Logging is not available in this context, but the error is handled
                // gracefully.
                (void)e; // Suppress unused variable warning
            }
        }

    } catch (const std::exception& e) {
        response.error_message = "Exception: " + std::string(e.what());
        response.error_code = LLMEngine::LLMEngineErrorCode::Network;
    }

    return response;
}

void GeminiClient::sendRequestStream(std::string_view prompt,
                                     const nlohmann::json& input,
                                     const nlohmann::json& params,
                                     LLMEngine::StreamCallback callback,
                                     const ::LLMEngine::RequestOptions& options) const {
    // Merge params
    nlohmann::json request_params = default_params_;
    if (!params.is_null()) {
        request_params.update(params);
    }

    // Build user text
    std::string user_text;
    if (input.contains(std::string(::LLMEngine::Constants::JsonKeys::SYSTEM_PROMPT))
        && input[std::string(::LLMEngine::Constants::JsonKeys::SYSTEM_PROMPT)].is_string()) {
        user_text += input[std::string(::LLMEngine::Constants::JsonKeys::SYSTEM_PROMPT)]
                         .get<std::string>();
        if (!user_text.empty() && user_text.back() != '\n')
            user_text += "\n";
    }
    user_text += std::string(prompt);

    auto buffer = std::make_shared<std::string>();

    ChatCompletionRequestHelper::executeStream(
        default_params_,
        params,
        // Build payload
        [&](const nlohmann::json& rp) {
            return nlohmann::json{
                {"contents",
                 nlohmann::json::array({nlohmann::json{
                     {"role", "user"},
                     {"parts", nlohmann::json::array({nlohmann::json{{"text", user_text}}})}}})},
                {"generationConfig",
                 nlohmann::json{{"temperature", rp["temperature"]},
                                {"maxOutputTokens", rp["max_tokens"]},
                                {"topP", rp["top_p"]}}}};
        },
        // Build URL (SSE mode)
        [&]() {
            return base_url_ + "/models/" + model_ + ":streamGenerateContent?alt=sse";
        },
        // Build Headers
        [&]() {
            return std::map<std::string, std::string>{{"Content-Type", "application/json"},
                                                      {"x-goog-api-key", api_key_}};
        },
        // Stream processor
        [buffer, callback](std::string_view chunk) {
            parseGeminiStreamChunk(chunk, *buffer, callback);
        },
        options,
        /*exponential_retry=*/false,
        config_.get());
    
    // Send final done signal
    LLMEngine::StreamChunk result;
    result.is_done = true;
    callback(result);
}

} // namespace LLMEngineAPI
