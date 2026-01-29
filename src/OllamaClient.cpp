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

#include <atomic>
#include <nlohmann/json.hpp>

namespace LLMEngineAPI {

namespace {
void parseOllamaStreamChunk(std::string_view chunk, std::string& buffer, const LLMEngine::StreamCallback& callback) {
    buffer.append(chunk);

    size_t pos = 0;
    while ((pos = buffer.find("\n")) != std::string::npos) {
        std::string line = buffer.substr(0, pos);
        buffer.erase(0, pos + 1);

        if (line.empty() || line == "\r")
            continue;

        try {
            auto json = nlohmann::json::parse(line);
            
            // Check for done
            bool done = json.value("done", false);
            
            if (json.contains("message") && json["message"].contains("content")) {
                std::string text = json["message"]["content"].get<std::string>();
                if (!text.empty()) {
                    LLMEngine::StreamChunk result;
                    result.content = text;
                    result.is_done = false;
                    callback(result);
                }
            } else if (json.contains("response")) {
                // Generate API format
                std::string text = json["response"].get<std::string>();
                if (!text.empty()) {
                    LLMEngine::StreamChunk result;
                    result.content = text;
                    result.is_done = false;
                    callback(result);
                }
            }

            if (done) {
                // Extract metrics if available
                if (json.contains("eval_count")) {
                    LLMEngine::StreamChunk result;
                    result.is_done = false;
                    result.usage = LLMEngine::AnalysisResult::UsageStats{
                        .promptTokens = json.value("prompt_eval_count", 0),
                        .completionTokens = json.value("eval_count", 0)
                    };
                    callback(result);
                }
                // Send explicit done signals are handled by executeStream finally block, 
                // but we can send one if we want to be explicit about closing based on payload.
                // However, let's let the stream end naturally.
            }

        } catch (...) {
            // Ignore
        }
    }
}
} // namespace

OllamaClient::OllamaClient(const std::string& base_url, const std::string& model)
    : baseUrl_(base_url), model_(model) {
    defaultParams_ = {{"temperature", ::LLMEngine::Constants::DefaultValues::TEMPERATURE},
                       {"top_p", ::LLMEngine::Constants::DefaultValues::TOP_P},
                       {"top_k", ::LLMEngine::Constants::DefaultValues::TOP_K},
                       {"min_p", ::LLMEngine::Constants::DefaultValues::MIN_P},
                       {"context_window", ::LLMEngine::Constants::DefaultValues::CONTEXT_WINDOW}};
}

APIResponse OllamaClient::sendRequest(std::string_view prompt,
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
        nlohmann::json request_params = defaultParams_;
        request_params.update(params);

        // Shared helpers
        auto get_timeout_seconds = [&](const nlohmann::json& p) -> int {
            if (p.contains(std::string(::LLMEngine::Constants::JsonKeys::TIMEOUT_SECONDS))) {
                return p.at(std::string(::LLMEngine::Constants::JsonKeys::TIMEOUT_SECONDS))
                    .get<int>();
            }
            return config_ ? config_->getTimeoutSeconds("ollama")
                           : APIConfigManager::getInstance().getTimeoutSeconds("ollama");
        };

        // SSL verification toggle (default: false for local Ollama, which often uses self-signed
        // certs) Set verify_ssl=true in params if you want to enforce SSL verification SECURITY:
        // Log when verification is disabled to prevent accidental use in production
        bool verify_ssl = false;
        if (params.contains("verify_ssl") && params.at("verify_ssl").is_boolean()) {
            verify_ssl = params.at("verify_ssl").get<bool>();
        }
        if (!verify_ssl) {
            // Log when TLS verification is disabled (Ollama defaults to false for local use)
            // This helps prevent accidental use in production environments
            // Use static flag to print warning only once per process
            static std::atomic<bool> warning_printed{false};
            bool expected = false;
            if (warning_printed.compare_exchange_strong(expected, true)) {
                std::cerr << "[LLMEngine SECURITY WARNING] TLS verification is DISABLED for Ollama "
                             "request. "
                          << "This is acceptable for local development but should be enabled in "
                             "production.\n";
            }
        }

        auto post_json = [&, verify_ssl](const std::string& url,
                                         const nlohmann::json& payload,
                                         int timeout_seconds) -> cpr::Response {
            std::map<std::string, std::string> hdr{{"Content-Type", "application/json"}};
            maybeLogRequest("POST", url, hdr);
            return sendWithRetries(
                rs,
                [&, verify_ssl]() {
                    return cpr::Post(cpr::Url{url},
                                     cpr::Header{hdr.begin(), hdr.end()},
                                     cpr::Body{payload.dump()},
                                     cpr::Timeout{timeout_seconds * MILLISECONDS_PER_SECOND},
                                     cpr::VerifySsl{verify_ssl});
                },
                options);
        };

        auto set_error_from_http = [&](APIResponse& out, const cpr::Response& r) {
            out.errorMessage = "HTTP " + std::to_string(r.status_code) + ": " + r.text;
            if (r.status_code == ::LLMEngine::HttpStatus::UNAUTHORIZED
                || r.status_code == ::LLMEngine::HttpStatus::FORBIDDEN) {
                out.errorCode = LLMEngine::LLMEngineErrorCode::Auth;
            } else if (r.status_code == ::LLMEngine::HttpStatus::TOO_MANY_REQUESTS) {
                out.errorCode = LLMEngine::LLMEngineErrorCode::RateLimited;
            } else if (::LLMEngine::HttpStatus::isServerError(static_cast<int>(r.status_code))) {
                out.errorCode = LLMEngine::LLMEngineErrorCode::Server;
            } else {
                out.errorCode = LLMEngine::LLMEngineErrorCode::Unknown;
            }
        };

        // Check if we should use generate mode instead of chat mode
        bool use_generate = false;
        if (params.contains(std::string(::LLMEngine::Constants::JsonKeys::MODE))
            && params[std::string(::LLMEngine::Constants::JsonKeys::MODE)] == "generate") {
            use_generate = true;
        }

        if (use_generate) {
            // Use generate API for text completion
            nlohmann::json payload = {{"model", model_}, {"prompt", prompt}, {"stream", false}};

            // Add parameters (filter out Ollama-specific ones)
            for (auto& [key, value] : request_params.items()) {
                if (key != "context_window") {
                    payload[key] = value;
                }
            }

            // Timeout selection
            const int timeout_seconds = get_timeout_seconds(params);
            const std::string url = baseUrl_ + "/api/generate";
            cpr::Response cpr_response = post_json(url, payload, timeout_seconds);

            response.statusCode = static_cast<int>(cpr_response.status_code);

            if (cpr_response.status_code == ::LLMEngine::HttpStatus::OK) {
                if (cpr_response.text.empty()) {
                    response.errorMessage = "Empty response from server";
                } else {
                    try {
                        response.rawResponse = nlohmann::json::parse(cpr_response.text);

                        if (response.rawResponse.contains("response")) {
                            response.content = response.rawResponse["response"].get<std::string>();
                            response.success = true;
                        } else {
                            response.errorMessage = "No response content in generate API response";
                        }
                    } catch (const nlohmann::json::parse_error& e) {
                        response.errorMessage = "JSON parse error: " + std::string(e.what())
                                                 + " - Response: " + cpr_response.text;
                        response.errorCode = LLMEngine::LLMEngineErrorCode::InvalidResponse;
                    }
                }
            } else {
                set_error_from_http(response, cpr_response);
            }
        } else {
            // Use chat API for conversational mode (default)
            // Use chat API for conversational mode (default)
            // Use shared builder to construct messages from input (handles system prompt, history, images)
            // Note: prompt arg is usually redundant if input has messages, but builder handles it.
            nlohmann::json messages = ChatMessageBuilder::buildMessages(prompt, input);

            // Prepare request payload for chat API
            nlohmann::json payload = {{"model", model_}, {"messages", messages}, {"stream", false}};

            // Add parameters (filter out Ollama-specific ones)
            for (auto& [key, value] : request_params.items()) {
                if (key != "context_window") {
                    payload[key] = value;
                }
            }

            const int timeout_seconds = get_timeout_seconds(params);
            const std::string url = baseUrl_ + "/api/chat";
            cpr::Response cpr_response = post_json(url, payload, timeout_seconds);

            response.statusCode = static_cast<int>(cpr_response.status_code);

            if (cpr_response.status_code == ::LLMEngine::HttpStatus::OK) {
                if (cpr_response.text.empty()) {
                    response.errorMessage = "Empty response from server";
                } else {
                    try {
                        response.rawResponse = nlohmann::json::parse(cpr_response.text);

                        if (response.rawResponse.contains("message")
                            && response.rawResponse["message"].contains("content")) {
                            response.content =
                                response.rawResponse["message"]["content"].get<std::string>();
                            response.success = true;
                        } else {
                            response.errorMessage = "No content in response";
                        }
                    } catch (const nlohmann::json::parse_error& e) {
                        response.errorMessage = "JSON parse error: " + std::string(e.what())
                                                 + " - Response: " + cpr_response.text;
                    }
                }
            } else {
                set_error_from_http(response, cpr_response);
            }
        }

    } catch (const std::exception& e) {
        response.errorMessage = "Exception: " + std::string(e.what());
        response.errorCode = LLMEngine::LLMEngineErrorCode::Network;
    }

    return response;
}

void OllamaClient::sendRequestStream(std::string_view prompt,
                                      const nlohmann::json& input,
                                      const nlohmann::json& params,
                                      LLMEngine::StreamCallback callback,
                                      const ::LLMEngine::RequestOptions& options) const {
    // Merge params
    nlohmann::json request_params = defaultParams_;
    if (!params.is_null()) {
        request_params.update(params);
    }

    bool use_generate = false;
    if (params.contains(std::string(::LLMEngine::Constants::JsonKeys::MODE))
        && params[std::string(::LLMEngine::Constants::JsonKeys::MODE)] == "generate") {
        use_generate = true;
    }

    // Prepare payload
    nlohmann::json payload;
    std::string endpoint;

    if (use_generate) {
        payload = {{"model", model_}, {"prompt", prompt}, {"stream", true}};
        endpoint = "/api/generate";
    } else {
        nlohmann::json messages = ChatMessageBuilder::buildMessages(prompt, input);
        payload = {{"model", model_}, {"messages", messages}, {"stream", true}};
        endpoint = "/api/chat";
    }

    // Add extra params
    for (auto& [key, value] : request_params.items()) {
        if (key != "context_window" && !payload.contains(key)) {
            payload[key] = value;
        }
    }

    auto buffer = std::make_shared<std::string>();

    ChatCompletionRequestHelper::executeStream(
        defaultParams_,
        params,
        // Build payload
        [&](const nlohmann::json&) {
            return payload;
        },
        // Build URL
        [&]() { return baseUrl_ + endpoint; },
        // Build Headers
        [&]() {
            return std::map<std::string, std::string>{{"Content-Type", "application/json"}};
        },
        // Stream processor
        [buffer, callback](std::string_view chunk) {
            parseOllamaStreamChunk(chunk, *buffer, callback);
        },
        options,
        /*exponential_retry=*/false,
        config_.get());

    LLMEngine::StreamChunk result;
    result.is_done = true;
    callback(result);
}

} // namespace LLMEngineAPI
