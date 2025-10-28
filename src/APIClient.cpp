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

namespace LLMEngineAPI {

// QwenClient Implementation
QwenClient::QwenClient(const std::string& api_key, const std::string& model)
    : api_key_(api_key), model_(model), base_url_("https://dashscope-intl.aliyuncs.com/compatible-mode/v1") {
    default_params_ = {
        {"temperature", 0.7},
        {"max_tokens", 2000},
        {"top_p", 0.9},
        {"frequency_penalty", 0.0},
        {"presence_penalty", 0.0}
    };
}

APIResponse QwenClient::sendRequest(const std::string& prompt, 
                                    const nlohmann::json& input,
                                    const nlohmann::json& params) const {
    APIResponse response;
    response.success = false;
    
    try {
        
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
        
        // Get timeout from params or use default (5 minutes)
        int timeout_seconds = 300; // Default 5 minutes
        if (params.contains("timeout_seconds")) {
            timeout_seconds = params["timeout_seconds"].get<int>();
        }
        
        // Send request
        auto cpr_response = cpr::Post(
            cpr::Url{base_url_ + "/chat/completions"},
            cpr::Header{{"Content-Type", "application/json"}, {"Authorization", "Bearer " + api_key_}},
            cpr::Body{payload.dump()},
            cpr::Timeout{timeout_seconds * 1000}
        );
        
        response.status_code = static_cast<int>(cpr_response.status_code);
        response.raw_response = nlohmann::json::parse(cpr_response.text);
        
        if (cpr_response.status_code == 200) {
            if (response.raw_response.contains("choices") && 
                response.raw_response["choices"].is_array() && 
                !response.raw_response["choices"].empty()) {
                
                auto choice = response.raw_response["choices"][0];
                if (choice.contains("message") && choice["message"].contains("content")) {
                    response.content = choice["message"]["content"].get<std::string>();
                    response.success = true;
                } else {
                    response.error_message = "No content in response";
                }
            } else {
                response.error_message = "Invalid response format";
            }
        } else {
            response.error_message = "HTTP " + std::to_string(cpr_response.status_code) + ": " + cpr_response.text;
        }
        
    } catch (const std::exception& e) {
        response.error_message = "Exception: " + std::string(e.what());
    }
    
    return response;
}

// OpenAIClient Implementation
OpenAIClient::OpenAIClient(const std::string& api_key, const std::string& model)
    : api_key_(api_key), model_(model), base_url_("https://api.openai.com/v1") {
    default_params_ = {
        {"temperature", 0.7},
        {"max_tokens", 2000},
        {"top_p", 0.9},
        {"frequency_penalty", 0.0},
        {"presence_penalty", 0.0}
    };
}

APIResponse OpenAIClient::sendRequest(const std::string& prompt, 
                                     const nlohmann::json& input,
                                     const nlohmann::json& params) const {
    APIResponse response;
    response.success = false;
    
    try {
        
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
        
        // Get timeout from params or use default (5 minutes)
        int timeout_seconds = 300; // Default 5 minutes
        if (params.contains("timeout_seconds")) {
            timeout_seconds = params["timeout_seconds"].get<int>();
        }
        
        // Send request
        auto cpr_response = cpr::Post(
            cpr::Url{base_url_ + "/chat/completions"},
            cpr::Header{{"Content-Type", "application/json"}, {"Authorization", "Bearer " + api_key_}},
            cpr::Body{payload.dump()},
            cpr::Timeout{timeout_seconds * 1000}
        );
        
        response.status_code = static_cast<int>(cpr_response.status_code);
        response.raw_response = nlohmann::json::parse(cpr_response.text);
        
        if (cpr_response.status_code == 200) {
            if (response.raw_response.contains("choices") && 
                response.raw_response["choices"].is_array() && 
                !response.raw_response["choices"].empty()) {
                
                auto choice = response.raw_response["choices"][0];
                if (choice.contains("message") && choice["message"].contains("content")) {
                    response.content = choice["message"]["content"].get<std::string>();
                    response.success = true;
                } else {
                    response.error_message = "No content in response";
                }
            } else {
                response.error_message = "Invalid response format";
            }
        } else {
            response.error_message = "HTTP " + std::to_string(cpr_response.status_code) + ": " + cpr_response.text;
        }
        
    } catch (const std::exception& e) {
        response.error_message = "Exception: " + std::string(e.what());
    }
    
    return response;
}

// AnthropicClient Implementation
AnthropicClient::AnthropicClient(const std::string& api_key, const std::string& model)
    : api_key_(api_key), model_(model), base_url_("https://api.anthropic.com/v1") {
    default_params_ = {
        {"max_tokens", 2000},
        {"temperature", 0.7},
        {"top_p", 0.9}
    };
}

APIResponse AnthropicClient::sendRequest(const std::string& prompt, 
                                        const nlohmann::json& input,
                                        const nlohmann::json& params) const {
    APIResponse response;
    response.success = false;
    
    try {
        
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
        
        // Get timeout from params or use default (5 minutes)
        int timeout_seconds = 300; // Default 5 minutes
        if (params.contains("timeout_seconds")) {
            timeout_seconds = params["timeout_seconds"].get<int>();
        }
        
        // Send request
        auto cpr_response = cpr::Post(
            cpr::Url{base_url_ + "/messages"},
            cpr::Header{{"Content-Type", "application/json"}, {"x-api-key", api_key_}, {"anthropic-version", "2023-06-01"}},
            cpr::Body{payload.dump()},
            cpr::Timeout{timeout_seconds * 1000}
        );
        
        response.status_code = static_cast<int>(cpr_response.status_code);
        response.raw_response = nlohmann::json::parse(cpr_response.text);
        
        if (cpr_response.status_code == 200) {
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
        {"temperature", 0.7},
        {"top_p", 0.9},
        {"top_k", 40},
        {"min_p", 0.05},
        {"context_window", 10000}
    };
}

APIResponse OllamaClient::sendRequest(const std::string& prompt, 
                                     const nlohmann::json& input,
                                     const nlohmann::json& params) const {
    APIResponse response;
    response.success = false;
    
    try {
        
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
            
            // Get timeout from params or use default (5 minutes)
            int timeout_seconds = 300; // Default 5 minutes
            if (params.contains("timeout_seconds")) {
                timeout_seconds = params["timeout_seconds"].get<int>();
            }
            
            // Send request to generate endpoint
            auto cpr_response = cpr::Post(
                cpr::Url{base_url_ + "/api/generate"},
                cpr::Header{{"Content-Type", "application/json"}},
                cpr::Body{payload.dump()},
                cpr::Timeout{timeout_seconds * 1000}
            );
            
            response.status_code = static_cast<int>(cpr_response.status_code);
            
            if (cpr_response.status_code == 200) {
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
                    }
                }
            } else {
                response.error_message = "HTTP " + std::to_string(cpr_response.status_code) + ": " + cpr_response.text;
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
        
        // Get timeout from params or use default (5 minutes)
        int timeout_seconds = 300; // Default 5 minutes
        if (params.contains("timeout_seconds")) {
            timeout_seconds = params["timeout_seconds"].get<int>();
        }
        
        // Send request to chat endpoint
        auto cpr_response = cpr::Post(
            cpr::Url{base_url_ + "/api/chat"},
            cpr::Header{{"Content-Type", "application/json"}},
            cpr::Body{payload.dump()},
            cpr::Timeout{timeout_seconds * 1000}
        );
        
        response.status_code = static_cast<int>(cpr_response.status_code);
        
        if (cpr_response.status_code == 200) {
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
        }
        } // End of else block for chat mode
        
    } catch (const std::exception& e) {
        response.error_message = "Exception: " + std::string(e.what());
    }
    
    return response;
}

// APIClientFactory Implementation
std::unique_ptr<APIClient> APIClientFactory::createClient(ProviderType type, 
                                                         const std::string& api_key,
                                                         const std::string& model,
                                                         const std::string& base_url) {
    switch (type) {
        case ProviderType::QWEN:
            return std::make_unique<QwenClient>(api_key, model);
        case ProviderType::OPENAI:
            return std::make_unique<OpenAIClient>(api_key, model);
        case ProviderType::ANTHROPIC:
            return std::make_unique<AnthropicClient>(api_key, model);
        case ProviderType::OLLAMA:
            return std::make_unique<OllamaClient>(base_url, model);
        default:
            return nullptr;
    }
}

std::unique_ptr<APIClient> APIClientFactory::createClientFromConfig(const std::string& provider_name,
                                                                     const nlohmann::json& config) {
    ProviderType type = stringToProviderType(provider_name);
    if (type == ProviderType::OLLAMA) {
        std::string base_url = config.value("base_url", "http://localhost:11434");
        std::string model = config.value("default_model", "llama2");
        return std::make_unique<OllamaClient>(base_url, model);
    } else {
        std::string api_key = config.value("api_key", "");
        std::string model = config.value("default_model", "");
        return createClient(type, api_key, model);
    }
}

ProviderType APIClientFactory::stringToProviderType(const std::string& provider_name) {
    if (provider_name == "qwen") return ProviderType::QWEN;
    if (provider_name == "openai") return ProviderType::OPENAI;
    if (provider_name == "anthropic") return ProviderType::ANTHROPIC;
    if (provider_name == "ollama") return ProviderType::OLLAMA;
    return ProviderType::OLLAMA; // Default fallback
}

std::string APIClientFactory::providerTypeToString(ProviderType type) {
    switch (type) {
        case ProviderType::QWEN: return "qwen";
        case ProviderType::OPENAI: return "openai";
        case ProviderType::ANTHROPIC: return "anthropic";
        case ProviderType::OLLAMA: return "ollama";
        default: return "ollama";
    }
}

// APIConfigManager Implementation
APIConfigManager& APIConfigManager::getInstance() {
    static APIConfigManager instance;
    return instance;
}

bool APIConfigManager::loadConfig(const std::string& config_path) {
    std::string path = config_path.empty() ? "config/api_config.json" : config_path;
    
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

nlohmann::json APIConfigManager::getProviderConfig(const std::string& provider_name) const {
    if (!config_loaded_) {
        return nlohmann::json{};
    }
    
    if (config_.contains("providers") && config_["providers"].contains(provider_name)) {
        return config_["providers"][provider_name];
    }
    
    return nlohmann::json{};
}

std::vector<std::string> APIConfigManager::getAvailableProviders() const {
    std::vector<std::string> providers;
    
    if (config_loaded_ && config_.contains("providers")) {
        for (auto& [key, value] : config_["providers"].items()) {
            providers.push_back(key);
        }
    }
    
    return providers;
}

std::string APIConfigManager::getDefaultProvider() const {
    if (config_loaded_ && config_.contains("default_provider")) {
        return config_["default_provider"].get<std::string>();
    }
    return "ollama";
}

int APIConfigManager::getTimeoutSeconds() const {
    if (config_loaded_ && config_.contains("timeout_seconds")) {
        return config_["timeout_seconds"].get<int>();
    }
    return 30;
}

int APIConfigManager::getRetryAttempts() const {
    if (config_loaded_ && config_.contains("retry_attempts")) {
        return config_["retry_attempts"].get<int>();
    }
    return 3;
}

int APIConfigManager::getRetryDelayMs() const {
    if (config_loaded_ && config_.contains("retry_delay_ms")) {
        return config_["retry_delay_ms"].get<int>();
    }
    return 1000;
}

} // namespace LLMEngineAPIAPI
