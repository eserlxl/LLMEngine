// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "APIClientCommon.hpp"
#include "ChatCompletionRequestHelper.hpp"
#include "LLMEngine/providers/APIClient.hpp"
#include "LLMEngine/core/Constants.hpp"
#include "LLMEngine/http/HttpStatus.hpp"

#include <nlohmann/json.hpp>

namespace LLMEngineAPI {

namespace {
void parseAnthropicStreamChunk(std::string_view chunk, std::string& buffer, const LLMEngine::StreamCallback& callback) {
    buffer.append(chunk);

    size_t pos = 0;
    while ((pos = buffer.find("\n")) != std::string::npos) {
        std::string line = buffer.substr(0, pos);
        buffer.erase(0, pos + 1);

        if (line.empty() || line == "\r")
            continue;


        constexpr size_t dataPrefixLen = 6;

        if (line.rfind("event: ", 0) == 0) {
            // Check event type
            // Event type usage logic ommitted for now as we parse data directly, 
            // but structure implies we might use it eventually)
        }
        
        if (line.rfind("data: ", 0) == 0) {
            std::string data = line.substr(dataPrefixLen);
            
            try {
                auto json = nlohmann::json::parse(data);
                
                std::string type;
                if (json.contains("type")) type = json["type"].get<std::string>();

                if (type == "content_block_delta") {
                    if (json.contains("delta") && json["delta"].contains("text")) {
                        std::string text = json["delta"]["text"].get<std::string>();
                        LLMEngine::StreamChunk result;
                        result.content = text;
                        result.is_done = false;
                        callback(result);
                    }
                } else if (type == "message_stop") {
                    // Done
                } else if (type == "message_delta") {
                    if (json.contains("usage")) {
                         LLMEngine::StreamChunk result;
                         result.is_done = false;
                         result.usage = LLMEngine::AnalysisResult::UsageStats{
                            .completionTokens = json["usage"].value("output_tokens", 0)
                         };
                         callback(result);
                    }
                }
            } catch (...) {
                // Ignore
            }
        }
    }
}
} // namespace

AnthropicClient::AnthropicClient(const std::string& api_key, const std::string& model)
    : apiKey_(api_key), model_(model),
      baseUrl_(std::string(::LLMEngine::Constants::DefaultUrls::ANTHROPIC_BASE)) {
    defaultParams_ = {{"max_tokens", ::LLMEngine::Constants::DefaultValues::MAX_TOKENS},
                       {"temperature", ::LLMEngine::Constants::DefaultValues::TEMPERATURE},
                       {"top_p", ::LLMEngine::Constants::DefaultValues::TOP_P}};
}

nlohmann::json AnthropicClient::buildPayload(std::string_view prompt,
                                             const nlohmann::json& input,
                                             const nlohmann::json& requestParams) const {
    // Prepare messages array
    nlohmann::json messages = nlohmann::json::array();
    messages.push_back({{"role", "user"}, {"content", prompt}});

    // Prepare request payload
    nlohmann::json payload = {{"model", model_},
                              {"max_tokens", requestParams["max_tokens"]},
                              {"temperature", requestParams["temperature"]},
                              {"top_p", requestParams["top_p"]},
                              {"messages", messages}};

    // Add system prompt if provided
    if (input.contains(std::string(::LLMEngine::Constants::JsonKeys::SYSTEM_PROMPT))) {
        payload["system"] = input[std::string(::LLMEngine::Constants::JsonKeys::SYSTEM_PROMPT)]
                                .get<std::string>();
    }
    return payload;
}

std::string AnthropicClient::buildUrl() const {
    return baseUrl_ + "/messages";
}

std::map<std::string, std::string> AnthropicClient::buildHeaders() const {
    return {{"Content-Type", "application/json"},
            {"x-api-key", apiKey_},
            {"anthropic-version", "2023-06-01"}};
}

APIResponse AnthropicClient::sendRequest(std::string_view prompt,
                                         const nlohmann::json& input,
                                         const nlohmann::json& params,
                                         const ::LLMEngine::RequestOptions& options) const {
    (void)options;
    APIResponse response;
    response.success = false;

    try {
        RetrySettings rs =
            computeRetrySettings(params, config_.get(), /*exponential_default*/ true);

        // Merge default params with provided params using update() for efficiency
        nlohmann::json requestParams = defaultParams_;
        requestParams.update(params);

        nlohmann::json payload = buildPayload(prompt, input, requestParams);

        // Get timeout from params or use config default
        int timeoutSeconds = 0;
        if (params.contains(std::string(::LLMEngine::Constants::JsonKeys::TIMEOUT_SECONDS))) {
            timeoutSeconds =
                params[std::string(::LLMEngine::Constants::JsonKeys::TIMEOUT_SECONDS)].get<int>();
        } else {
            timeoutSeconds = config_ ? config_->getTimeoutSeconds()
                                      : APIConfigManager::getInstance().getTimeoutSeconds();
        }
        // Clamp to safe range [1, MAX_TIMEOUT_SECONDS] seconds
        timeoutSeconds = std::max(timeoutSeconds, 1);
        timeoutSeconds =
            std::min(timeoutSeconds, ::LLMEngine::Constants::DefaultValues::MAX_TIMEOUT_SECONDS);

        // SSL verification toggle (default: true for security)
        bool verifySsl = true;
        if (params.contains("verify_ssl") && params.at("verify_ssl").is_boolean()) {
            verifySsl = params.at("verify_ssl").get<bool>();
        }

        // Send request with retries
        const std::string url = buildUrl();
        auto hdr = buildHeaders();

        maybeLogRequest("POST", url, hdr);
        cpr::Response cprResponse = sendWithRetries(
            rs,
            [&]() {
                return cpr::Post(cpr::Url{url},
                                 cpr::Header{hdr.begin(), hdr.end()},
                                 cpr::Body{payload.dump()},
                                 cpr::Timeout{timeoutSeconds * MILLISECONDS_PER_SECOND},
                                 cpr::VerifySsl{verifySsl});
            },
            options);

        response.statusCode = static_cast<int>(cprResponse.status_code);

        if (cprResponse.status_code == ::LLMEngine::HttpStatus::OK) {
            // Parse JSON only after confirming HTTP success
            try {
                response.rawResponse = nlohmann::json::parse(cprResponse.text);
                if (response.rawResponse.contains("content")
                    && response.rawResponse["content"].is_array()
                    && !response.rawResponse["content"].empty()) {

                    auto content = response.rawResponse["content"][0];
                    if (content.contains("text")) {
                        response.content = content["text"].get<std::string>();
                        response.success = true;
                    } else {
                        response.errorMessage = "No text content in response";
                        response.errorCode = LLMEngine::LLMEngineErrorCode::InvalidResponse;
                    }
                } else {
                    response.errorMessage = "Invalid response format";
                    response.errorCode = LLMEngine::LLMEngineErrorCode::InvalidResponse;
                }
            } catch (const nlohmann::json::parse_error& e) {
                response.errorMessage = "JSON parse error: " + std::string(e.what());
                response.errorCode = LLMEngine::LLMEngineErrorCode::InvalidResponse;
            }
        } else {
            // Handle error responses - try to parse JSON for structured error messages
            response.errorMessage =
                "HTTP " + std::to_string(cprResponse.status_code) + ": " + cprResponse.text;
            try {
                response.rawResponse = nlohmann::json::parse(cprResponse.text);
            } catch (const nlohmann::json::parse_error&) {
                // Non-JSON error response (e.g., HTML error page) - keep text error message
                response.rawResponse = nlohmann::json::object();
            }

            // Classify error based on HTTP status code
            if (cprResponse.status_code == ::LLMEngine::HttpStatus::UNAUTHORIZED
                || cprResponse.status_code == ::LLMEngine::HttpStatus::FORBIDDEN) {
                response.errorCode = LLMEngine::LLMEngineErrorCode::Auth;
            } else if (cprResponse.status_code == ::LLMEngine::HttpStatus::TOO_MANY_REQUESTS) {
                response.errorCode = LLMEngine::LLMEngineErrorCode::RateLimited;
            } else if (::LLMEngine::HttpStatus::isServerError(
                           static_cast<int>(cprResponse.status_code))) {
                response.errorCode = LLMEngine::LLMEngineErrorCode::Server;
            } else {
                response.errorCode = LLMEngine::LLMEngineErrorCode::Unknown;
            }
        }

    } catch (const nlohmann::json::parse_error& e) {
        // This catch block should rarely be reached now, but kept for safety
        response.errorMessage = "JSON parse error: " + std::string(e.what());
        response.errorCode = LLMEngine::LLMEngineErrorCode::InvalidResponse;
    } catch (const std::exception& e) {
        response.errorMessage = "Exception: " + std::string(e.what());
        response.errorCode = LLMEngine::LLMEngineErrorCode::Network;
    }

    return response;
}

void AnthropicClient::sendRequestStream(std::string_view prompt,
                                          const nlohmann::json& input,
                                          const nlohmann::json& params,
                                          LLMEngine::StreamCallback callback,
                                          const ::LLMEngine::RequestOptions& options) const {
    auto buffer = std::make_shared<std::string>();

    ChatCompletionRequestHelper::executeStream(
        defaultParams_,
        params,
        // Build payload
        [&](const nlohmann::json& rp) {
            nlohmann::json payload = buildPayload(prompt, input, rp);
            payload["stream"] = true;
            return payload;
        },
        // Build URL
        [&]() { return buildUrl(); },
        // Build Headers
        [&]() { return buildHeaders(); },
        // Stream processor
        [buffer, callback](std::string_view chunk) {
            parseAnthropicStreamChunk(chunk, *buffer, callback);
        },
        options,
        /*exponential_retry=*/true,
        config_.get());

    LLMEngine::StreamChunk result;
    result.is_done = true;
    callback(result);
}

} // namespace LLMEngineAPI
