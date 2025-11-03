// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "APIClient.hpp"
#include <cpr/cpr.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <cstring>
#include <random>
#include "Backoff.hpp"

namespace LLMEngineAPI {

// Constants
namespace {
    constexpr double DEFAULT_TEMPERATURE = 0.7;
    constexpr int DEFAULT_MAX_TOKENS = 2000;
    constexpr double DEFAULT_TOP_P = 0.9;
    constexpr double DEFAULT_MIN_P = 0.05;
    constexpr int DEFAULT_TOP_K = 40;
    constexpr int DEFAULT_CONTEXT_WINDOW = 10000;
    constexpr int MILLISECONDS_PER_SECOND = 1000;
    constexpr int HTTP_STATUS_OK = 200;
    constexpr int HTTP_STATUS_UNAUTHORIZED = 401;
    constexpr int HTTP_STATUS_FORBIDDEN = 403;
    constexpr int HTTP_STATUS_TOO_MANY_REQUESTS = 429;
    constexpr int HTTP_STATUS_SERVER_ERROR_MIN = 500;
    constexpr int DEFAULT_TIMEOUT_SECONDS = 30;
    constexpr int DEFAULT_RETRY_ATTEMPTS = 3;
    constexpr int DEFAULT_RETRY_DELAY_MS = 1000;
    constexpr int DEFAULT_MAX_BACKOFF_DELAY_MS = 30000; // maximum cap for backoff delays in milliseconds
}

// QwenClient Implementation
QwenClient::QwenClient(const std::string& api_key, const std::string& model)
    : api_key_(api_key), model_(model), base_url_("https://dashscope-intl.aliyuncs.com/compatible-mode/v1") {
    default_params_ = {
        {"temperature", DEFAULT_TEMPERATURE},
        {"max_tokens", DEFAULT_MAX_TOKENS},
        {"top_p", DEFAULT_TOP_P},
        {"frequency_penalty", 0.0},
        {"presence_penalty", 0.0}
    };
}

APIResponse QwenClient::sendRequest(std::string_view prompt, 
                                    const nlohmann::json& input,
                                    const nlohmann::json& params) const {
    APIResponse response;
    response.success = false;
    response.error_code = APIResponse::APIError::Unknown;
    
    try {
        int max_attempts = std::max(1, APIConfigManager::getInstance().getRetryAttempts());
        int base_delay_ms = std::max(0, APIConfigManager::getInstance().getRetryDelayMs());
        int max_delay_ms = DEFAULT_MAX_BACKOFF_DELAY_MS;
        uint64_t jitter_seed = 0;
        if (params.contains("retry_attempts")) max_attempts = std::max(1, params["retry_attempts"].get<int>());
        if (params.contains("retry_base_delay_ms")) base_delay_ms = std::max(0, params["retry_base_delay_ms"].get<int>());
        if (params.contains("retry_max_delay_ms")) max_delay_ms = std::max(0, params["retry_max_delay_ms"].get<int>());
        if (params.contains("jitter_seed")) jitter_seed = params["jitter_seed"].get<uint64_t>();
        BackoffConfig bcfg{base_delay_ms, max_delay_ms};
        std::mt19937_64 rng(jitter_seed);
        
        // Merge default params with provided params
        nlohmann::json request_params = default_params_;
        for (auto& [key, value] : params.items()) {
            request_params[key] = value;
        }
        
        // Prepare messages array
        nlohmann::json messages = nlohmann::json::array();
        
        // Add system message if input contains system prompt
        if (input.contains("system_prompt")) {
            messages.push_back({
                {"role", "system"},
                {"content", input["system_prompt"].get<std::string>()}
            });
        }
        
        // Add user message
        messages.push_back({
            {"role", "user"},
            {"content", prompt}
        });
        
        // Prepare request payload
        nlohmann::json payload = {
            {"model", model_},
            {"messages", messages},
            {"temperature", request_params["temperature"]},
            {"max_tokens", request_params["max_tokens"]},
            {"top_p", request_params["top_p"]},
            {"frequency_penalty", request_params["frequency_penalty"]},
            {"presence_penalty", request_params["presence_penalty"]}
        };
        
        // Get timeout from params or use config default
        int timeout_seconds = 0;
        if (params.contains("timeout_seconds")) {
            timeout_seconds = params["timeout_seconds"].get<int>();
        } else {
            timeout_seconds = APIConfigManager::getInstance().getTimeoutSeconds();
        }
        
        cpr::Response cpr_response;
        for (int attempt = 1; attempt <= max_attempts; ++attempt) {
            cpr_response = cpr::Post(
                cpr::Url{base_url_ + "/chat/completions"},
                cpr::Header{{"Content-Type", "application/json"}, {"Authorization", "Bearer " + api_key_}},
                cpr::Body{payload.dump()},
                cpr::Timeout{timeout_seconds * MILLISECONDS_PER_SECOND}
            );
            if (cpr_response.status_code == HTTP_STATUS_OK && !cpr_response.text.empty()) break;
            if (attempt < max_attempts) {
                const uint64_t cap = computeBackoffCapMs(bcfg, attempt);
                const int delay = jitterDelayMs(rng, cap);
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            }
        }
        
        response.status_code = static_cast<int>(cpr_response.status_code);
        
        if (cpr_response.status_code == HTTP_STATUS_OK) {
            // Only parse JSON for successful responses
            try {
                response.raw_response = nlohmann::json::parse(cpr_response.text);
                
                if (response.raw_response.contains("choices") && 
                    response.raw_response["choices"].is_array() && 
                    !response.raw_response["choices"].empty()) {
                    
                    auto choice = response.raw_response["choices"][0];
                    if (choice.contains("message") && choice["message"].contains("content")) {
                        response.content = choice["message"]["content"].get<std::string>();
                        response.success = true;
                    } else {
                        response.error_message = "No content in response";
                        response.error_code = APIResponse::APIError::InvalidResponse;
                    }
                } else {
                    response.error_message = "Invalid response format";
                    response.error_code = APIResponse::APIError::InvalidResponse;
                }
            } catch (const nlohmann::json::parse_error& e) {
                response.error_message = "JSON parse error in successful response: " + std::string(e.what());
                response.error_code = APIResponse::APIError::InvalidResponse;
            }
        } else {
            // For error responses, attempt to parse JSON but don't fail if it's not JSON
            response.error_message = "HTTP " + std::to_string(cpr_response.status_code) + ": " + cpr_response.text;
            if (cpr_response.status_code == HTTP_STATUS_UNAUTHORIZED || cpr_response.status_code == HTTP_STATUS_FORBIDDEN) {
                response.error_code = APIResponse::APIError::Auth;
            } else if (cpr_response.status_code == HTTP_STATUS_TOO_MANY_REQUESTS) {
                response.error_code = APIResponse::APIError::RateLimited;
            } else {
                response.error_code = APIResponse::APIError::Server;
            }
            
            // Attempt to parse error response JSON if available
            if (!cpr_response.text.empty()) {
                try {
                    response.raw_response = nlohmann::json::parse(cpr_response.text);
                } catch (const nlohmann::json::parse_error&) {  // NOLINT(bugprone-empty-catch): Intentionally ignoring parse errors for non-JSON error responses
                    // Non-JSON error response is acceptable - keep raw_response as default empty json
                    // error_message already contains the text
                }
            }
        }
        
    } catch (const std::exception& e) {
        response.error_message = "Exception: " + std::string(e.what());
        response.error_code = APIResponse::APIError::Network;
    }
    
    return response;
}

// OpenAIClient Implementation
OpenAIClient::OpenAIClient(const std::string& api_key, const std::string& model)
    : api_key_(api_key), model_(model), base_url_("https://api.openai.com/v1") {
    default_params_ = {
        {"temperature", DEFAULT_TEMPERATURE},
        {"max_tokens", DEFAULT_MAX_TOKENS},
        {"top_p", DEFAULT_TOP_P},
        {"frequency_penalty", 0.0},
        {"presence_penalty", 0.0}
    };
}

APIResponse OpenAIClient::sendRequest(std::string_view prompt, 
                                     const nlohmann::json& input,
                                     const nlohmann::json& params) const {
    APIResponse response;
    response.success = false;
    
    try {
        int max_attempts = std::max(1, APIConfigManager::getInstance().getRetryAttempts());
        int base_delay_ms = std::max(0, APIConfigManager::getInstance().getRetryDelayMs());
        int max_delay_ms = DEFAULT_MAX_BACKOFF_DELAY_MS;
        uint64_t jitter_seed = 0;
        if (params.contains("retry_attempts")) max_attempts = std::max(1, params["retry_attempts"].get<int>());
        if (params.contains("retry_base_delay_ms")) base_delay_ms = std::max(0, params["retry_base_delay_ms"].get<int>());
        if (params.contains("retry_max_delay_ms")) max_delay_ms = std::max(0, params["retry_max_delay_ms"].get<int>());
        if (params.contains("jitter_seed")) jitter_seed = params["jitter_seed"].get<uint64_t>();
        BackoffConfig bcfg{base_delay_ms, max_delay_ms};
        std::mt19937_64 rng(jitter_seed);
        
        // Merge default params with provided params
        nlohmann::json request_params = default_params_;
        for (auto& [key, value] : params.items()) {
            request_params[key] = value;
        }
        
        // Prepare messages array
        nlohmann::json messages = nlohmann::json::array();
        
        // Add system message if input contains system prompt
        if (input.contains("system_prompt")) {
            messages.push_back({
                {"role", "system"},
                {"content", input["system_prompt"].get<std::string>()}
            });
        }
        
        // Add user message
        messages.push_back({
            {"role", "user"},
            {"content", prompt}
        });
        
        // Prepare request payload
        nlohmann::json payload = {
            {"model", model_},
            {"messages", messages},
            {"temperature", request_params["temperature"]},
            {"max_tokens", request_params["max_tokens"]},
            {"top_p", request_params["top_p"]},
            {"frequency_penalty", request_params["frequency_penalty"]},
            {"presence_penalty", request_params["presence_penalty"]}
        };
        
        // Get timeout from params or use config default
        int timeout_seconds = 0;
        if (params.contains("timeout_seconds")) {
            timeout_seconds = params["timeout_seconds"].get<int>();
        } else {
            timeout_seconds = APIConfigManager::getInstance().getTimeoutSeconds();
        }
        
        cpr::Response cpr_response;
        for (int attempt = 1; attempt <= max_attempts; ++attempt) {
            cpr_response = cpr::Post(
                cpr::Url{base_url_ + "/chat/completions"},
                cpr::Header{{"Content-Type", "application/json"}, {"Authorization", "Bearer " + api_key_}},
                cpr::Body{payload.dump()},
                cpr::Timeout{timeout_seconds * MILLISECONDS_PER_SECOND}
            );
            if (cpr_response.status_code == HTTP_STATUS_OK && !cpr_response.text.empty()) break;
            if (attempt < max_attempts) {
                const uint64_t cap = computeBackoffCapMs(bcfg, attempt);
                const int delay = jitterDelayMs(rng, cap);
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            }
        }
        
        response.status_code = static_cast<int>(cpr_response.status_code);
        response.raw_response = nlohmann::json::parse(cpr_response.text);
        
        if (cpr_response.status_code == HTTP_STATUS_OK) {
            if (response.raw_response.contains("choices") && 
                response.raw_response["choices"].is_array() && 
                !response.raw_response["choices"].empty()) {
                
                auto choice = response.raw_response["choices"][0];
                if (choice.contains("message") && choice["message"].contains("content")) {
                    response.content = choice["message"]["content"].get<std::string>();
                    response.success = true;
                } else {
                    response.error_message = "No content in response";
                    response.error_code = APIResponse::APIError::InvalidResponse;
                }
            } else {
                response.error_message = "Invalid response format";
                response.error_code = APIResponse::APIError::InvalidResponse;
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
        
    } catch (const std::exception& e) {
        response.error_message = "Exception: " + std::string(e.what());
        response.error_code = APIResponse::APIError::Network;
    }
    
    return response;
}

// AnthropicClient Implementation
AnthropicClient::AnthropicClient(const std::string& api_key, const std::string& model)
    : api_key_(api_key), model_(model), base_url_("https://api.anthropic.com/v1") {
    default_params_ = {
        {"max_tokens", DEFAULT_MAX_TOKENS},
        {"temperature", DEFAULT_TEMPERATURE},
        {"top_p", DEFAULT_TOP_P}
    };
}

APIResponse AnthropicClient::sendRequest(std::string_view prompt, 
                                        const nlohmann::json& input,
                                        const nlohmann::json& params) const {
    APIResponse response;
    response.success = false;
    
    try {
        int max_attempts = std::max(1, APIConfigManager::getInstance().getRetryAttempts());
        int base_delay_ms = std::max(0, APIConfigManager::getInstance().getRetryDelayMs());
        int max_delay_ms = DEFAULT_MAX_BACKOFF_DELAY_MS;
        uint64_t jitter_seed = 0;
        if (params.contains("retry_attempts")) max_attempts = std::max(1, params["retry_attempts"].get<int>());
        if (params.contains("retry_base_delay_ms")) base_delay_ms = std::max(0, params["retry_base_delay_ms"].get<int>());
        if (params.contains("retry_max_delay_ms")) max_delay_ms = std::max(0, params["retry_max_delay_ms"].get<int>());
        if (params.contains("jitter_seed")) jitter_seed = params["jitter_seed"].get<uint64_t>();
        BackoffConfig bcfg{base_delay_ms, max_delay_ms};
        std::mt19937_64 rng(jitter_seed);
        
        // Merge default params with provided params
        nlohmann::json request_params = default_params_;
        for (auto& [key, value] : params.items()) {
            request_params[key] = value;
        }
        
        // Prepare messages array
        nlohmann::json messages = nlohmann::json::array();
        messages.push_back({
            {"role", "user"},
            {"content", prompt}
        });
        
        // Prepare request payload
        nlohmann::json payload = {
            {"model", model_},
            {"max_tokens", request_params["max_tokens"]},
            {"temperature", request_params["temperature"]},
            {"top_p", request_params["top_p"]},
            {"messages", messages}
        };
        
        // Add system prompt if provided
        if (input.contains("system_prompt")) {
            payload["system"] = input["system_prompt"].get<std::string>();
        }
        
        // Get timeout from params or use config default
        int timeout_seconds = 0;
        if (params.contains("timeout_seconds")) {
            timeout_seconds = params["timeout_seconds"].get<int>();
        } else {
            timeout_seconds = APIConfigManager::getInstance().getTimeoutSeconds();
        }
        
        // Send request with retries
        cpr::Response cpr_response;
        for (int attempt = 1; attempt <= max_attempts; ++attempt) {
            cpr_response = cpr::Post(
                cpr::Url{base_url_ + "/messages"},
                cpr::Header{{"Content-Type", "application/json"}, {"x-api-key", api_key_}, {"anthropic-version", "2023-06-01"}},
                cpr::Body{payload.dump()},
                cpr::Timeout{timeout_seconds * MILLISECONDS_PER_SECOND}
            );
            if (cpr_response.status_code == HTTP_STATUS_OK && !cpr_response.text.empty()) break;
            if (attempt < max_attempts) {
                const uint64_t cap = computeBackoffCapMs(bcfg, attempt);
                const int delay = jitterDelayMs(rng, cap);
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            }
        }
        
        response.status_code = static_cast<int>(cpr_response.status_code);
        response.raw_response = nlohmann::json::parse(cpr_response.text);
        
        if (cpr_response.status_code == HTTP_STATUS_OK) {
            if (response.raw_response.contains("content") && 
                response.raw_response["content"].is_array() && 
                !response.raw_response["content"].empty()) {
                
                auto content = response.raw_response["content"][0];
                if (content.contains("text")) {
                    response.content = content["text"].get<std::string>();
                    response.success = true;
                } else {
                    response.error_message = "No text content in response";
                }
            } else {
                response.error_message = "Invalid response format";
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
        
    } catch (const std::exception& e) {
        response.error_message = "Exception: " + std::string(e.what());
    }
    
    return response;
}

// OllamaClient Implementation
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
        int max_attempts = std::max(1, APIConfigManager::getInstance().getRetryAttempts());
        int base_delay_ms = std::max(0, APIConfigManager::getInstance().getRetryDelayMs());
        int max_delay_ms = DEFAULT_MAX_BACKOFF_DELAY_MS;
        uint64_t jitter_seed = 0;
        if (params.contains("retry_attempts")) max_attempts = std::max(1, params["retry_attempts"].get<int>());
        if (params.contains("retry_base_delay_ms")) base_delay_ms = std::max(0, params["retry_base_delay_ms"].get<int>());
        if (params.contains("retry_max_delay_ms")) max_delay_ms = std::max(0, params["retry_max_delay_ms"].get<int>());
        if (params.contains("jitter_seed")) jitter_seed = params["jitter_seed"].get<uint64_t>();
        BackoffConfig bcfg{base_delay_ms, max_delay_ms};
        std::mt19937_64 rng(jitter_seed);
        
        // Merge default params with provided params
        nlohmann::json request_params = default_params_;
        for (auto& [key, value] : params.items()) {
            request_params[key] = value;
        }
        
        // Check if we should use generate mode instead of chat mode
        bool use_generate = false;
        if (params.contains("mode") && params["mode"] == "generate") {
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
                if (key != "context_window") {  // Skip context_window as it's not a valid Ollama parameter
                    payload[key] = value;
                }
            }
            
            // Get timeout from params or use provider-specific/default config
            int timeout_seconds = 0;
            if (params.contains("timeout_seconds")) {
                timeout_seconds = params["timeout_seconds"].get<int>();
            } else {
                // Use provider-specific timeout (300s for Ollama) or global default (30s)
                timeout_seconds = APIConfigManager::getInstance().getTimeoutSeconds("ollama");
            }
            
            cpr::Response cpr_response;
            for (int attempt = 1; attempt <= max_attempts; ++attempt) {
                cpr_response = cpr::Post(
                    cpr::Url{base_url_ + "/api/generate"},
                    cpr::Header{{"Content-Type", "application/json"}},
                    cpr::Body{payload.dump()},
                    cpr::Timeout{timeout_seconds * MILLISECONDS_PER_SECOND}
                );
                if (cpr_response.status_code == HTTP_STATUS_OK && !cpr_response.text.empty()) break;
                if (attempt < max_attempts) {
                    int delay = base_delay_ms * attempt;
                    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
                }
            }
            
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
            // Prepare messages array for chat API
        nlohmann::json messages = nlohmann::json::array();
        
        // Add system message if input contains system prompt
        if (input.contains("system_prompt")) {
            messages.push_back({
                {"role", "system"},
                {"content", input["system_prompt"].get<std::string>()}
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
            if (key != "context_window") {  // Skip context_window as it's not a valid Ollama parameter
                payload[key] = value;
            }
        }
        
        // Get timeout from params or use provider-specific/default config
        int timeout_seconds = 0;
        if (params.contains("timeout_seconds")) {
            timeout_seconds = params["timeout_seconds"].get<int>();
        } else {
            // Use provider-specific timeout (300s for Ollama) or global default (30s)
            timeout_seconds = APIConfigManager::getInstance().getTimeoutSeconds("ollama");
        }
        
        cpr::Response cpr_response;
        for (int attempt = 1; attempt <= max_attempts; ++attempt) {
            cpr_response = cpr::Post(
                cpr::Url{base_url_ + "/api/chat"},
                cpr::Header{{"Content-Type", "application/json"}},
                cpr::Body{payload.dump()},
                cpr::Timeout{timeout_seconds * MILLISECONDS_PER_SECOND}
            );
            if (cpr_response.status_code == HTTP_STATUS_OK && !cpr_response.text.empty()) break;
            if (attempt < max_attempts) {
                const uint64_t cap = computeBackoffCapMs(bcfg, attempt);
                const int delay = jitterDelayMs(rng, cap);
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            }
        }
        
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
        } // End of else block for chat mode
        
    } catch (const std::exception& e) {
        response.error_message = "Exception: " + std::string(e.what());
        response.error_code = APIResponse::APIError::Network;
    }
    
    return response;
}

// GeminiClient Implementation
GeminiClient::GeminiClient(const std::string& api_key, const std::string& model)
    : api_key_(api_key), model_(model), base_url_("https://generativelanguage.googleapis.com/v1beta") {
    default_params_ = {
        {"temperature", DEFAULT_TEMPERATURE},
        {"max_tokens", DEFAULT_MAX_TOKENS},
        {"top_p", DEFAULT_TOP_P}
    };
}

APIResponse GeminiClient::sendRequest(std::string_view prompt,
                                     const nlohmann::json& input,
                                     const nlohmann::json& params) const {
    APIResponse response;
    response.success = false;

    try {
        const int max_attempts = std::max(1, APIConfigManager::getInstance().getRetryAttempts());
        const int base_delay_ms = std::max(0, APIConfigManager::getInstance().getRetryDelayMs());

        // Merge default params with provided params
        nlohmann::json request_params = default_params_;
        for (auto& [key, value] : params.items()) {
            request_params[key] = value;
        }

        // Compose user content; prepend optional system_prompt
        std::string user_text;
        if (input.contains("system_prompt") && input["system_prompt"].is_string()) {
            user_text += input["system_prompt"].get<std::string>();
            if (!user_text.empty() && user_text.back() != '\n') user_text += "\n";
        }
        user_text += std::string(prompt);

        // Prepare request payload for Gemini generateContent
        nlohmann::json payload = {
            {"contents", nlohmann::json::array({ nlohmann::json{
                {"role", "user"},
                {"parts", nlohmann::json::array({ nlohmann::json{{"text", user_text}} })}
            } })},
            {"generationConfig", nlohmann::json{
                {"temperature", request_params["temperature"]},
                {"maxOutputTokens", request_params["max_tokens"]},
                {"topP", request_params["top_p"]}
            }}
        };

        // Get timeout from params or use config default
        int timeout_seconds = 0;
        if (params.contains("timeout_seconds")) {
            timeout_seconds = params["timeout_seconds"].get<int>();
        } else {
            timeout_seconds = APIConfigManager::getInstance().getTimeoutSeconds();
        }

        const std::string url = base_url_ + "/models/" + model_ + ":generateContent?key=" + api_key_;

        cpr::Response cpr_response;
        for (int attempt = 1; attempt <= max_attempts; ++attempt) {
            cpr_response = cpr::Post(
                cpr::Url{url},
                cpr::Header{{"Content-Type", "application/json"}},
                cpr::Body{payload.dump()},
                cpr::Timeout{timeout_seconds * MILLISECONDS_PER_SECOND}
            );
            if (cpr_response.status_code == HTTP_STATUS_OK && !cpr_response.text.empty()) break;
            if (attempt < max_attempts) {
                int delay = base_delay_ms * attempt;
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            }
        }

        response.status_code = static_cast<int>(cpr_response.status_code);

        if (cpr_response.status_code == HTTP_STATUS_OK) {
            if (cpr_response.text.empty()) {
                response.error_message = "Empty response from server";
                response.error_code = APIResponse::APIError::InvalidResponse;
            } else {
                try {
                    response.raw_response = nlohmann::json::parse(cpr_response.text);
                    if (response.raw_response.contains("candidates") &&
                        response.raw_response["candidates"].is_array() &&
                        !response.raw_response["candidates"].empty()) {
                        const auto& cand = response.raw_response["candidates"][0];
                        std::string aggregated;
                        if (cand.contains("content") && cand["content"].contains("parts") && cand["content"]["parts"].is_array()) {
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
                            response.error_code = APIResponse::APIError::InvalidResponse;
                        }
                    } else {
                        response.error_message = "Invalid response format";
                        response.error_code = APIResponse::APIError::InvalidResponse;
                    }
                } catch (const nlohmann::json::parse_error& e) {
                    response.error_message = std::string("JSON parse error: ") + e.what();
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
            try {
                response.raw_response = nlohmann::json::parse(cpr_response.text);
            } catch (const std::exception& e) {
                // keep raw_response default, ignore parse errors
                static_cast<void>(e);
            }
        }

    } catch (const std::exception& e) {
        response.error_message = "Exception: " + std::string(e.what());
        response.error_code = APIResponse::APIError::Network;
    }

    return response;
}

// APIClientFactory Implementation
std::unique_ptr<APIClient> APIClientFactory::createClient(ProviderType type, 
                                                         std::string_view api_key,
                                                         std::string_view model,
                                                         std::string_view base_url) {
    switch (type) {
        case ProviderType::QWEN:
            return std::make_unique<QwenClient>(std::string(api_key), std::string(model));
        case ProviderType::OPENAI:
            return std::make_unique<OpenAIClient>(std::string(api_key), std::string(model));
        case ProviderType::ANTHROPIC:
            return std::make_unique<AnthropicClient>(std::string(api_key), std::string(model));
        case ProviderType::OLLAMA:
            return std::make_unique<OllamaClient>(std::string(base_url), std::string(model));
        case ProviderType::GEMINI:
            return std::make_unique<GeminiClient>(std::string(api_key), std::string(model));
        default:
            return nullptr;
    }
}

std::unique_ptr<APIClient> APIClientFactory::createClientFromConfig(std::string_view provider_name,
                                                                     const nlohmann::json& config) {
    ProviderType type = stringToProviderType(provider_name);
    if (type == ProviderType::OLLAMA) {
        std::string base_url = config.value("base_url", "http://localhost:11434");
        std::string model = config.value("default_model", "llama2");
        return std::make_unique<OllamaClient>(base_url, model);
    }
    
    // SECURITY: Prefer environment variables for API keys over config file
    // Environment variable names: QWEN_API_KEY, OPENAI_API_KEY, ANTHROPIC_API_KEY, GEMINI_API_KEY
    std::string api_key = config.value("api_key", "");
    std::string env_var_name;
    switch (type) {
        case ProviderType::QWEN: env_var_name = "QWEN_API_KEY"; break;
        case ProviderType::OPENAI: env_var_name = "OPENAI_API_KEY"; break;
        case ProviderType::ANTHROPIC: env_var_name = "ANTHROPIC_API_KEY"; break;
        case ProviderType::GEMINI: env_var_name = "GEMINI_API_KEY"; break;
        default: break;
    }
    
    // Override config API key with environment variable if available
    const char* env_api_key = std::getenv(env_var_name.c_str());
    if (env_api_key && strlen(env_api_key) > 0) {
        api_key = env_api_key;
    } else if (!api_key.empty()) {
        // Warn if falling back to config file for credentials
        std::cerr << "[WARNING] Using API key from config file. For production use, "
                  << "set the " << env_var_name << " environment variable instead. "
                  << "Storing credentials in config files is a security risk." << std::endl;
    }
    
    std::string model = config.value("default_model", "");
    return createClient(type, api_key, model);
}

ProviderType APIClientFactory::stringToProviderType(std::string_view provider_name) {
    std::string name(provider_name);
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    
    // Direct mapping for known providers
    if (name == "qwen") return ProviderType::QWEN;
    if (name == "openai") return ProviderType::OPENAI;
    if (name == "anthropic") return ProviderType::ANTHROPIC;
    if (name == "ollama") return ProviderType::OLLAMA;
    if (name == "gemini") return ProviderType::GEMINI;
    
    // SECURITY: Fail fast on unknown providers instead of silently falling back
    // This prevents unexpected provider selection in multi-tenant contexts
    // If an empty or invalid provider is provided, throw an exception rather than defaulting
    if (!name.empty()) {
        throw std::runtime_error("Unknown provider: " + name + 
                                 ". Supported providers: qwen, openai, anthropic, ollama, gemini");
    }
    
    // Only use default for empty string (explicit empty provider request)
    // This maintains backward compatibility while preventing silent failures
    const auto& cfg = APIConfigManager::getInstance();
    const std::string def = cfg.getDefaultProvider();
    std::string def_lower = def;
    std::transform(def_lower.begin(), def_lower.end(), def_lower.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    
    if (def_lower == "qwen") return ProviderType::QWEN;
    if (def_lower == "openai") return ProviderType::OPENAI;
    if (def_lower == "anthropic") return ProviderType::ANTHROPIC;
    if (def_lower == "ollama") return ProviderType::OLLAMA;
    if (def_lower == "gemini") return ProviderType::GEMINI;
    
    // Last resort: use Ollama as safe default only if default provider is also invalid
    return ProviderType::OLLAMA;
}

std::string APIClientFactory::providerTypeToString(ProviderType type) {
    switch (type) {
        case ProviderType::QWEN: return "qwen";
        case ProviderType::OPENAI: return "openai";
        case ProviderType::ANTHROPIC: return "anthropic";
        case ProviderType::OLLAMA: return "ollama";
        case ProviderType::GEMINI: return "gemini";
        default: return "ollama";
    }
}

// APIConfigManager Implementation
APIConfigManager::APIConfigManager() : default_config_path_("config/api_config.json") {
}

APIConfigManager& APIConfigManager::getInstance() {
    static APIConfigManager instance;
    return instance;
}

void APIConfigManager::setDefaultConfigPath(std::string_view config_path) {
    // Exclusive lock for writing default path
    std::unique_lock<std::shared_mutex> lock(mutex_);
    default_config_path_ = std::string(config_path);
}

std::string APIConfigManager::getDefaultConfigPath() const {
    // Shared lock for reading default path
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return default_config_path_;
}

bool APIConfigManager::loadConfig(std::string_view config_path) {
    std::string path;
    if (config_path.empty()) {
        // Use stored default path - read it first with shared lock
        {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            path = default_config_path_;
        }
        // Then acquire exclusive lock for writing configuration
    } else {
        path = std::string(config_path);
    }
    
    // Exclusive lock for writing configuration
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "[ERROR] Could not open config file: " << path << std::endl;
            return false;
        }
        
        file >> config_;
        config_loaded_ = true;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to load config: " << e.what() << std::endl;
        return false;
    }
}

nlohmann::json APIConfigManager::getProviderConfig(std::string_view provider_name) const {
    // Shared lock for reading configuration
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    if (!config_loaded_) {
        return nlohmann::json{};
    }
    
    if (config_.contains("providers")) {
        std::string key(provider_name);
        if (config_["providers"].contains(key)) {
            return config_["providers"][key];
        }
    }
    
    return nlohmann::json{};
}

std::vector<std::string> APIConfigManager::getAvailableProviders() const {
    // Shared lock for reading configuration
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::vector<std::string> providers;
    
    if (config_loaded_ && config_.contains("providers")) {
        for (auto& [key, value] : config_["providers"].items()) {
            providers.push_back(key);
        }
    }
    
    return providers;
}

std::string APIConfigManager::getDefaultProvider() const {
    // Shared lock for reading configuration
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    if (config_loaded_ && config_.contains("default_provider")) {
        return config_["default_provider"].get<std::string>();
    }
    return "ollama";
}

int APIConfigManager::getTimeoutSeconds() const {
    // Shared lock for reading configuration
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    if (config_loaded_ && config_.contains("timeout_seconds")) {
        return config_["timeout_seconds"].get<int>();
    }
    return DEFAULT_TIMEOUT_SECONDS;
}

int APIConfigManager::getTimeoutSeconds(std::string_view provider_name) const {
    // Shared lock for reading configuration
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    // Check for provider-specific timeout first
    if (config_loaded_ && config_.contains("providers")) {
        std::string provider_key(provider_name);
        if (config_["providers"].contains(provider_key)) {
            const auto& provider_config = config_["providers"][provider_key];
            if (provider_config.contains("timeout_seconds") && provider_config["timeout_seconds"].is_number_integer()) {
                return provider_config["timeout_seconds"].get<int>();
            }
        }
    }
    
    // Fall back to global timeout
    return getTimeoutSeconds();
}

int APIConfigManager::getRetryAttempts() const {
    // Shared lock for reading configuration
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    if (config_loaded_ && config_.contains("retry_attempts")) {
        return config_["retry_attempts"].get<int>();
    }
    return DEFAULT_RETRY_ATTEMPTS;
}

int APIConfigManager::getRetryDelayMs() const {
    // Shared lock for reading configuration
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    if (config_loaded_ && config_.contains("retry_delay_ms")) {
        return config_["retry_delay_ms"].get<int>();
    }
    return DEFAULT_RETRY_DELAY_MS;
}

} // namespace LLMEngineAPI
