// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/APIClient.hpp"
#include "APIClientCommon.hpp"
#include "ChatCompletionRequestHelper.hpp"
#include "LLMEngine/Constants.hpp"
#include <nlohmann/json.hpp>

namespace LLMEngineAPI {

OpenAIClient::OpenAIClient(const std::string& api_key, const std::string& model)
    : api_key_(api_key), model_(model), base_url_(std::string(::LLMEngine::Constants::DefaultUrls::OPENAI_BASE)) {
    default_params_ = {
        {"temperature", ::LLMEngine::Constants::DefaultValues::TEMPERATURE},
        {"max_tokens", ::LLMEngine::Constants::DefaultValues::MAX_TOKENS},
        {"top_p", ::LLMEngine::Constants::DefaultValues::TOP_P},
        {"frequency_penalty", 0.0},
        {"presence_penalty", 0.0}
    };
}

APIResponse OpenAIClient::sendRequest(std::string_view prompt, 
                                     const nlohmann::json& input,
                                     const nlohmann::json& params) const {
    // Build messages array using shared helper
    const nlohmann::json messages = ChatMessageBuilder::buildMessages(prompt, input);
    
    // Use shared request helper for common lifecycle
    return ChatCompletionRequestHelper::execute(
        default_params_,
        params,
        // Build payload
        [&](const nlohmann::json& request_params) {
            return nlohmann::json{
                {"model", model_},
                {"messages", messages},
                {"temperature", request_params["temperature"]},
                {"max_tokens", request_params["max_tokens"]},
                {"top_p", request_params["top_p"]},
                {"frequency_penalty", request_params["frequency_penalty"]},
                {"presence_penalty", request_params["presence_penalty"]}
            };
        },
        // Build URL
        [&]() {
            return base_url_ + "/chat/completions";
        },
        // Build headers
        [&]() {
            return std::map<std::string, std::string>{
                {"Content-Type", "application/json"},
                {"Authorization", "Bearer " + api_key_}
            };
        },
        // Parse response (OpenAI-compatible format)
        [](APIResponse& response, const std::string&) {
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
        },
        /*exponential_retry*/ true
    );
}

} // namespace LLMEngineAPI

