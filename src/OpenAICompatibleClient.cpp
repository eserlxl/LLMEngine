// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "OpenAICompatibleClient.hpp"
#include <string>

namespace LLMEngineAPI {

OpenAICompatibleClient::OpenAICompatibleClient(const std::string& api_key, 
                                               const std::string& model,
                                               const std::string& base_url)
    : api_key_(api_key), model_(model), base_url_(base_url) {
    // Initialize default parameters
    default_params_ = {
        {"temperature", ::LLMEngine::Constants::DefaultValues::TEMPERATURE},
        {"max_tokens", ::LLMEngine::Constants::DefaultValues::MAX_TOKENS},
        {"top_p", ::LLMEngine::Constants::DefaultValues::TOP_P},
        {"frequency_penalty", 0.0},
        {"presence_penalty", 0.0}
    };
    
    // Cache the chat completions URL to avoid string concatenation on every request
    chat_completions_url_.reserve(base_url_.size() + 18);  // "/chat/completions" = 18 chars
    chat_completions_url_ = base_url_;
    chat_completions_url_ += "/chat/completions";
    
    // Cache headers to avoid rebuilding on every request
    std::string auth_header;
    auth_header.reserve(api_key_.size() + 7);  // "Bearer " = 7 chars
    auth_header = "Bearer ";
    auth_header += api_key_;
    
    cached_headers_ = {
        {"Content-Type", "application/json"},
        {"Authorization", std::move(auth_header)}
    };
}

const std::map<std::string, std::string>& OpenAICompatibleClient::getHeaders() const {
    return cached_headers_;  // Return cached headers
}

std::string OpenAICompatibleClient::buildUrl() const {
    return chat_completions_url_;  // Return cached URL
}

nlohmann::json OpenAICompatibleClient::buildPayload(const nlohmann::json& messages, 
                                                    const nlohmann::json& request_params) const {
    return nlohmann::json{
        {"model", model_},
        {"messages", messages},
        {"temperature", request_params["temperature"]},
        {"max_tokens", request_params["max_tokens"]},
        {"top_p", request_params["top_p"]},
        {"frequency_penalty", request_params["frequency_penalty"]},
        {"presence_penalty", request_params["presence_penalty"]}
    };
}

void OpenAICompatibleClient::parseOpenAIResponse(APIResponse& response, const std::string&) {
    if (response.raw_response.contains("choices") && 
        response.raw_response["choices"].is_array() && 
        !response.raw_response["choices"].empty()) {
        
        auto choice = response.raw_response["choices"][0];
        if (choice.contains("message") && choice["message"].contains("content")) {
            response.content = choice["message"]["content"].get<std::string>();
            response.success = true;
        } else {
            response.error_message = "No content in response";
            response.error_code = LLMEngine::LLMEngineErrorCode::InvalidResponse;
        }
    } else {
        response.error_message = "Invalid response format";
        response.error_code = LLMEngine::LLMEngineErrorCode::InvalidResponse;
    }
}

} // namespace LLMEngineAPI

