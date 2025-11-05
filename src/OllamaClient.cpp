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
        {"temperature", ::LLMEngine::Constants::DefaultValues::TEMPERATURE},
        {"top_p", ::LLMEngine::Constants::DefaultValues::TOP_P},
        {"top_k", ::LLMEngine::Constants::DefaultValues::TOP_K},
        {"min_p", ::LLMEngine::Constants::DefaultValues::MIN_P},
        {"context_window", ::LLMEngine::Constants::DefaultValues::CONTEXT_WINDOW}
    };
}

APIResponse OllamaClient::sendRequest(std::string_view prompt, 
                                     const nlohmann::json& input,
                                     const nlohmann::json& params) const {
    APIResponse response;
    response.success = false;
    
    try {
        RetrySettings rs = computeRetrySettings(params, config_.get(), /*exponential_default*/false);
        
        // Merge default params with provided params using update() for efficiency
        nlohmann::json request_params = default_params_;
        request_params.update(params);

        // Shared helpers
        auto get_timeout_seconds = [&](const nlohmann::json& p) -> int {
            if (p.contains(std::string(::LLMEngine::Constants::JsonKeys::TIMEOUT_SECONDS))) {
                return p.at(std::string(::LLMEngine::Constants::JsonKeys::TIMEOUT_SECONDS)).get<int>();
            }
            return config_ ? config_->getTimeoutSeconds("ollama") : APIConfigManager::getInstance().getTimeoutSeconds("ollama");
        };

        // SSL verification toggle (default: false for local Ollama, which often uses self-signed certs)
        // Set verify_ssl=true in params if you want to enforce SSL verification
        bool verify_ssl = false;
        if (params.contains("verify_ssl") && params.at("verify_ssl").is_boolean()) {
            verify_ssl = params.at("verify_ssl").get<bool>();
        }

        auto post_json = [&, verify_ssl](const std::string& url, const nlohmann::json& payload, int timeout_seconds) -> cpr::Response {
            std::map<std::string, std::string> hdr{{"Content-Type", "application/json"}};
            maybeLogRequest("POST", url, hdr);
            return sendWithRetries(rs, [&, verify_ssl](){
                return cpr::Post(
                    cpr::Url{url},
                    cpr::Header{hdr.begin(), hdr.end()},
                    cpr::Body{payload.dump()},
                    cpr::Timeout{timeout_seconds * MILLISECONDS_PER_SECOND},
                    cpr::VerifySsl{verify_ssl}
                );
            });
        };

        auto set_error_from_http = [&](APIResponse& out, const cpr::Response& r){
            out.error_message = "HTTP " + std::to_string(r.status_code) + ": " + r.text;
            if (r.status_code == HTTP_STATUS_UNAUTHORIZED || r.status_code == HTTP_STATUS_FORBIDDEN) {
                out.error_code = APIResponse::APIError::Auth;
            } else if (r.status_code == HTTP_STATUS_TOO_MANY_REQUESTS) {
                out.error_code = APIResponse::APIError::RateLimited;
            } else if (r.status_code >= HTTP_STATUS_SERVER_ERROR_MIN) {
                out.error_code = APIResponse::APIError::Server;
            } else {
                out.error_code = APIResponse::APIError::Unknown;
            }
        };
        
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
            
            // Timeout selection
            const int timeout_seconds = get_timeout_seconds(params);
            const std::string url = base_url_ + "/api/generate";
            cpr::Response cpr_response = post_json(url, payload, timeout_seconds);
            
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
                set_error_from_http(response, cpr_response);
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
            
            const int timeout_seconds = get_timeout_seconds(params);
            const std::string url = base_url_ + "/api/chat";
            cpr::Response cpr_response = post_json(url, payload, timeout_seconds);
            
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
                set_error_from_http(response, cpr_response);
            }
        }
        
    } catch (const std::exception& e) {
        response.error_message = "Exception: " + std::string(e.what());
        response.error_code = APIResponse::APIError::Network;
    }
    
    return response;
}

} // namespace LLMEngineAPI

