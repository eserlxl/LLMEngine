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

OpenAIClient::OpenAIClient(const std::string& api_key, const std::string& model)
    : api_key_(api_key), model_(model), base_url_(std::string(::LLMEngine::Constants::DefaultUrls::OPENAI_BASE)) {
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
        RetrySettings rs = computeRetrySettings(params, /*exponential_default*/true);
        
        // Merge default params with provided params using update() for efficiency
        nlohmann::json request_params = default_params_;
        request_params.update(params);
        
        // Prepare messages array
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
        if (params.contains(std::string(::LLMEngine::Constants::JsonKeys::TIMEOUT_SECONDS))) {
            timeout_seconds = params[std::string(::LLMEngine::Constants::JsonKeys::TIMEOUT_SECONDS)].get<int>();
        } else {
            timeout_seconds = APIConfigManager::getInstance().getTimeoutSeconds();
        }
        
        const std::string url = base_url_ + "/chat/completions";
        std::map<std::string, std::string> hdr{{"Content-Type", "application/json"}, {"Authorization", "Bearer " + api_key_}};
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

} // namespace LLMEngineAPI

