// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/APIClient.hpp"
#include "APIClientCommon.hpp"
#include "LLMEngine/Constants.hpp"
#include <nlohmann/json.hpp>

namespace LLMEngineAPI {

OllamaClient::OllamaClient(const std::string& base_url, const std::string& model)
    : base_url_(base_url), model_(model) {
    default_params_ = {
        {"temperature", DEFAULT_TEMPERATURE},
        {"top_p", DEFAULT_TOP_P},
        {"top_k", DEFAULT_TOP_K},
        {"min_p", DEFAULT_MIN_P},
        {"context_window", DEFAULT_CONTEXT_WINDOW}
    };
}

APIResponse OllamaClient::sendRequest(std::string_view prompt, 
                                     const nlohmann::json& input,
                                     const nlohmann::json& params) const {
    APIResponse response;
    response.success = false;
    
    try {
        RetrySettings rs = computeRetrySettings(params, /*exponential_default*/false);
        
        // Merge default params with provided params using update() for efficiency
        nlohmann::json request_params = default_params_;
        request_params.update(params);
        
        // Check if we should use generate mode instead of chat mode
        bool use_generate = false;
        if (params.contains(std::string(::LLMEngine::Constants::JsonKeys::MODE)) && params[std::string(::LLMEngine::Constants::JsonKeys::MODE)] == "generate") {
            use_generate = true;
        }
        
        if (use_generate) {
            // Use generate API for text completion
            nlohmann::json payload = {
                {"model", model_},
                {"prompt", prompt},
                {"stream", false}
            };
            
            // Add parameters (filter out Ollama-specific ones)
            for (auto& [key, value] : request_params.items()) {
                if (key != "context_window") {
                    payload[key] = value;
                }
            }
            
            // Get timeout from params or use provider-specific/default config
            int timeout_seconds = 0;
        if (params.contains(std::string(::LLMEngine::Constants::JsonKeys::TIMEOUT_SECONDS))) {
                timeout_seconds = params[std::string(::LLMEngine::Constants::JsonKeys::TIMEOUT_SECONDS)].get<int>();
            } else {
            timeout_seconds = config_ ? config_->getTimeoutSeconds("ollama") : APIConfigManager::getInstance().getTimeoutSeconds("ollama");
            }
            
            const std::string url = base_url_ + "/api/generate";
            std::map<std::string, std::string> hdr{{"Content-Type", "application/json"}};
            maybeLogRequest("POST", url, hdr);
            cpr::Response cpr_response = sendWithRetries(rs, [&](){
                return cpr::Post(
                    cpr::Url{url},
                    cpr::Header{hdr.begin(), hdr.end()},
                    cpr::Body{payload.dump()},
                    cpr::Timeout{timeout_seconds * MILLISECONDS_PER_SECOND}
                );
            });
            
            response.status_code = static_cast<int>(cpr_response.status_code);
            
            if (cpr_response.status_code == HTTP_STATUS_OK) {
                if (cpr_response.text.empty()) {
                    response.error_message = "Empty response from server";
                } else {
                    try {
                        response.raw_response = nlohmann::json::parse(cpr_response.text);
                        
                        if (response.raw_response.contains("response")) {
                            response.content = response.raw_response["response"].get<std::string>();
                            response.success = true;
                        } else {
                            response.error_message = "No response content in generate API response";
                        }
                    } catch (const nlohmann::json::parse_error& e) {
                        response.error_message = "JSON parse error: " + std::string(e.what()) + " - Response: " + cpr_response.text;
                        response.error_code = APIResponse::APIError::InvalidResponse;
                    }
                }
            } else {
                response.error_message = "HTTP " + std::to_string(cpr_response.status_code) + ": " + cpr_response.text;
                if (cpr_response.status_code == HTTP_STATUS_UNAUTHORIZED || cpr_response.status_code == HTTP_STATUS_FORBIDDEN) {
                    response.error_code = APIResponse::APIError::Auth;
                } else if (cpr_response.status_code == HTTP_STATUS_TOO_MANY_REQUESTS) {
                    response.error_code = APIResponse::APIError::RateLimited;
                } else {
                    response.error_code = APIResponse::APIError::Server;
                }
            }
        } else {
            // Use chat API for conversational mode (default)
            nlohmann::json messages = nlohmann::json::array();
            
            // Add system message if input contains system prompt
            if (input.contains(std::string(::LLMEngine::Constants::JsonKeys::SYSTEM_PROMPT))) {
                messages.push_back({
                    {"role", "system"},
                    {"content", input[std::string(::LLMEngine::Constants::JsonKeys::SYSTEM_PROMPT)].get<std::string>()}
                });
            }
            
            // Add user message
            messages.push_back({
                {"role", "user"},
                {"content", prompt}
            });
            
            // Prepare request payload for chat API
            nlohmann::json payload = {
                {"model", model_},
                {"messages", messages},
                {"stream", false}
            };
            
            // Add parameters (filter out Ollama-specific ones)
            for (auto& [key, value] : request_params.items()) {
                if (key != "context_window") {
                    payload[key] = value;
                }
            }
            
            // Get timeout from params or use provider-specific/default config
            int timeout_seconds = 0;
            if (params.contains(std::string(::LLMEngine::Constants::JsonKeys::TIMEOUT_SECONDS))) {
                timeout_seconds = params[std::string(::LLMEngine::Constants::JsonKeys::TIMEOUT_SECONDS)].get<int>();
            } else {
                timeout_seconds = APIConfigManager::getInstance().getTimeoutSeconds("ollama");
            }
            
            const std::string url = base_url_ + "/api/chat";
            std::map<std::string, std::string> hdr{{"Content-Type", "application/json"}};
            maybeLogRequest("POST", url, hdr);
            cpr::Response cpr_response = sendWithRetries(rs, [&](){
                return cpr::Post(
                    cpr::Url{url},
                    cpr::Header{hdr.begin(), hdr.end()},
                    cpr::Body{payload.dump()},
                    cpr::Timeout{timeout_seconds * MILLISECONDS_PER_SECOND}
                );
            });
            
            response.status_code = static_cast<int>(cpr_response.status_code);
            
            if (cpr_response.status_code == HTTP_STATUS_OK) {
                if (cpr_response.text.empty()) {
                    response.error_message = "Empty response from server";
                } else {
                    try {
                        response.raw_response = nlohmann::json::parse(cpr_response.text);
                        
                        if (response.raw_response.contains("message") && 
                            response.raw_response["message"].contains("content")) {
                            response.content = response.raw_response["message"]["content"].get<std::string>();
                            response.success = true;
                        } else {
                            response.error_message = "No content in response";
                        }
                    } catch (const nlohmann::json::parse_error& e) {
                        response.error_message = "JSON parse error: " + std::string(e.what()) + " - Response: " + cpr_response.text;
                    }
                }
            } else {
                response.error_message = "HTTP " + std::to_string(cpr_response.status_code) + ": " + cpr_response.text;
                if (cpr_response.status_code == HTTP_STATUS_UNAUTHORIZED || cpr_response.status_code == HTTP_STATUS_FORBIDDEN) {
                    response.error_code = APIResponse::APIError::Auth;
                } else if (cpr_response.status_code == HTTP_STATUS_TOO_MANY_REQUESTS) {
                    response.error_code = APIResponse::APIError::RateLimited;
                } else if (cpr_response.status_code >= HTTP_STATUS_SERVER_ERROR_MIN) {
                    response.error_code = APIResponse::APIError::Server;
                } else {
                    response.error_code = APIResponse::APIError::Unknown;
                }
            }
        }
        
    } catch (const std::exception& e) {
        response.error_message = "Exception: " + std::string(e.what());
        response.error_code = APIResponse::APIError::Network;
    }
    
    return response;
}

} // namespace LLMEngineAPI

