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
void parseAnthropicStreamChunk(std::string_view chunk, std::string& buffer, const LLMEngine::StreamCallback& callback) {
    buffer.append(chunk);

    size_t pos = 0;
    while ((pos = buffer.find("\n")) != std::string::npos) {
        std::string line = buffer.substr(0, pos);
        buffer.erase(0, pos + 1);

        if (line.empty() || line == "\r")
            continue;

        constexpr size_t kEventPrefixLen = 7;
        constexpr size_t kDataPrefixLen = 6;

        if (line.rfind("event: ", 0) == 0) {
            // Check event type
            // std::string event = line.substr(kEventPrefixLen);
            // (Event type usage logic ommitted for now as we parse data directly, 
            // but structure implies we might use it eventually)
        }
        
        if (line.rfind("data: ", 0) == 0) {
            std::string data = line.substr(kDataPrefixLen);
            
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
    : api_key_(api_key), model_(model),
      base_url_(std::string(::LLMEngine::Constants::DefaultUrls::ANTHROPIC_BASE)) {
    default_params_ = {{"max_tokens", ::LLMEngine::Constants::DefaultValues::MAX_TOKENS},
                       {"temperature", ::LLMEngine::Constants::DefaultValues::TEMPERATURE},
                       {"top_p", ::LLMEngine::Constants::DefaultValues::TOP_P}};
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
        nlohmann::json request_params = default_params_;
        request_params.update(params);

        // Prepare messages array
        nlohmann::json messages = nlohmann::json::array();
        messages.push_back({{"role", "user"}, {"content", prompt}});

        // Prepare request payload
        nlohmann::json payload = {{"model", model_},
                                  {"max_tokens", request_params["max_tokens"]},
                                  {"temperature", request_params["temperature"]},
                                  {"top_p", request_params["top_p"]},
                                  {"messages", messages}};

        // Add system prompt if provided
        if (input.contains(std::string(::LLMEngine::Constants::JsonKeys::SYSTEM_PROMPT))) {
            payload["system"] = input[std::string(::LLMEngine::Constants::JsonKeys::SYSTEM_PROMPT)]
                                    .get<std::string>();
        }

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

        // Send request with retries
        const std::string url = base_url_ + "/messages";
        std::map<std::string, std::string> hdr{{"Content-Type", "application/json"},
                                               {"x-api-key", api_key_},
                                               {"anthropic-version", "2023-06-01"}};
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
            // Parse JSON only after confirming HTTP success
            try {
                response.raw_response = nlohmann::json::parse(cpr_response.text);
                if (response.raw_response.contains("content")
                    && response.raw_response["content"].is_array()
                    && !response.raw_response["content"].empty()) {

                    auto content = response.raw_response["content"][0];
                    if (content.contains("text")) {
                        response.content = content["text"].get<std::string>();
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
                response.error_message = "JSON parse error: " + std::string(e.what());
                response.error_code = LLMEngine::LLMEngineErrorCode::InvalidResponse;
            }
        } else {
            // Handle error responses - try to parse JSON for structured error messages
            response.error_message =
                "HTTP " + std::to_string(cpr_response.status_code) + ": " + cpr_response.text;
            try {
                response.raw_response = nlohmann::json::parse(cpr_response.text);
            } catch (const nlohmann::json::parse_error&) {
                // Non-JSON error response (e.g., HTML error page) - keep text error message
                response.raw_response = nlohmann::json::object();
            }

            // Classify error based on HTTP status code
            if (cpr_response.status_code == ::LLMEngine::HttpStatus::UNAUTHORIZED
                || cpr_response.status_code == ::LLMEngine::HttpStatus::FORBIDDEN) {
                response.error_code = LLMEngine::LLMEngineErrorCode::Auth;
            } else if (cpr_response.status_code == ::LLMEngine::HttpStatus::TOO_MANY_REQUESTS) {
                response.error_code = LLMEngine::LLMEngineErrorCode::RateLimited;
            } else if (::LLMEngine::HttpStatus::isServerError(
                           static_cast<int>(cpr_response.status_code))) {
                response.error_code = LLMEngine::LLMEngineErrorCode::Server;
            } else {
                response.error_code = LLMEngine::LLMEngineErrorCode::Unknown;
            }
        }

    } catch (const nlohmann::json::parse_error& e) {
        // This catch block should rarely be reached now, but kept for safety
        response.error_message = "JSON parse error: " + std::string(e.what());
        response.error_code = LLMEngine::LLMEngineErrorCode::InvalidResponse;
    } catch (const std::exception& e) {
        response.error_message = "Exception: " + std::string(e.what());
        response.error_code = LLMEngine::LLMEngineErrorCode::Network;
    }

    return response;
}

void AnthropicClient::sendRequestStream(std::string_view prompt,
                                          const nlohmann::json& input,
                                          const nlohmann::json& params,
                                          LLMEngine::StreamCallback callback,
                                          const ::LLMEngine::RequestOptions& options) const {
    // Merge params
    nlohmann::json request_params = default_params_;
    if (!params.is_null()) {
        request_params.update(params);
    }

    nlohmann::json messages = nlohmann::json::array();
    messages.push_back({{"role", "user"}, {"content", prompt}});

    auto buffer = std::make_shared<std::string>();

    ChatCompletionRequestHelper::executeStream(
        default_params_,
        params,
        // Build payload
        [&](const nlohmann::json& rp) {
            nlohmann::json payload = {
                {"model", model_},
                {"max_tokens", rp["max_tokens"]},
                {"temperature", rp["temperature"]},
                {"top_p", rp["top_p"]},
                {"messages", messages},
                {"stream", true}
            };
            if (input.contains(std::string(::LLMEngine::Constants::JsonKeys::SYSTEM_PROMPT))) {
                payload["system"] = input[std::string(::LLMEngine::Constants::JsonKeys::SYSTEM_PROMPT)]
                                        .get<std::string>();
            }
            return payload;
        },
        // Build URL
        [&]() { return base_url_ + "/messages"; },
        // Build Headers
        [&]() {
            return std::map<std::string, std::string>{
                {"Content-Type", "application/json"},
                {"x-api-key", api_key_},
                {"anthropic-version", "2023-06-01"}};
        },
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
