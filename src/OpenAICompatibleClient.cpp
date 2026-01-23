// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "OpenAICompatibleClient.hpp"

#include <string>

namespace LLMEngineAPI {

namespace {
constexpr size_t CHAT_COMPLETIONS_PATH_LENGTH = 18; // "/chat/completions" = 18 chars
constexpr size_t BEARER_PREFIX_LENGTH = 7;          // "Bearer " = 7 chars
} // namespace

OpenAICompatibleClient::OpenAICompatibleClient(const std::string& api_key,
                                               const std::string& model,
                                               const std::string& base_url)
    : api_key_(api_key), model_(model), base_url_(base_url) {
    // Initialize default parameters
    default_params_ = {{"temperature", ::LLMEngine::Constants::DefaultValues::TEMPERATURE},
                       {"max_tokens", ::LLMEngine::Constants::DefaultValues::MAX_TOKENS},
                       {"top_p", ::LLMEngine::Constants::DefaultValues::TOP_P},
                       {"frequency_penalty", 0.0},
                       {"presence_penalty", 0.0}};

    // Cache the chat completions URL to avoid string concatenation on every request
    chat_completions_url_.reserve(base_url_.size() + CHAT_COMPLETIONS_PATH_LENGTH);
    chat_completions_url_ = base_url_;
    chat_completions_url_ += "/chat/completions";

    // Cache headers to avoid rebuilding on every request
    std::string auth_header;
    auth_header.reserve(api_key_.size() + BEARER_PREFIX_LENGTH);
    auth_header = "Bearer ";
    auth_header += api_key_;

    cached_headers_ = {{"Content-Type", "application/json"},
                       {"Authorization", std::move(auth_header)}};
}

const std::map<std::string, std::string>& OpenAICompatibleClient::getHeaders() const {
    return cached_headers_; // Return cached headers
}

std::string OpenAICompatibleClient::buildUrl() const {
    return chat_completions_url_; // Return cached URL
}

nlohmann::json OpenAICompatibleClient::buildPayload(const nlohmann::json& messages,
                                                    const nlohmann::json& request_params) const {
    return nlohmann::json{{"model", model_},
                          {"messages", messages},
                          {"temperature", request_params["temperature"]},
                          {"max_tokens", request_params["max_tokens"]},
                          {"top_p", request_params["top_p"]},
                          {"frequency_penalty", request_params["frequency_penalty"]},
                          {"presence_penalty", request_params["presence_penalty"]}};
}

void OpenAICompatibleClient::parseOpenAIResponse(APIResponse& response,
                                                 const std::string& /*unused*/) {
    if (response.raw_response.contains("choices") && response.raw_response["choices"].is_array()
        && !response.raw_response["choices"].empty()) {

        auto choice = response.raw_response["choices"][0];
        if (choice.contains("message") && choice["message"].contains("content")) {
            response.content = choice["message"]["content"].get<std::string>();
            response.success = true;

            // Parse token usage if available
            if (response.raw_response.contains("usage")
                && response.raw_response["usage"].is_object()) {
                const auto& usage = response.raw_response["usage"];
                if (usage.contains("prompt_tokens") && usage["prompt_tokens"].is_number_integer()) {
                    response.usage.promptTokens = usage["prompt_tokens"].get<int>();
                }
                if (usage.contains("completion_tokens")
                    && usage["completion_tokens"].is_number_integer()) {
                    response.usage.completionTokens = usage["completion_tokens"].get<int>();
                }
                if (usage.contains("total_tokens") && usage["total_tokens"].is_number_integer()) {
                    response.usage.totalTokens = usage["total_tokens"].get<int>();
                }
            }
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

void LLMEngineAPI::OpenAICompatibleClient::parseOpenAIStreamChunk(
    std::string_view chunk, std::string& buffer, std::function<void(std::string_view)> callback) {
    buffer.append(chunk);

    size_t pos = 0;
    while ((pos = buffer.find("\n\n")) != std::string::npos) {
        std::string line = buffer.substr(0, pos);
        buffer.erase(0, pos + 2); // Remove processed line + delimiter

        if (line.rfind("data: ", 0) == 0) {
            std::string data = line.substr(6); // Skip "data: "

            if (data == "[DONE]") {
                // Stream finished
                continue;
            }

            try {
                auto json = nlohmann::json::parse(data);
                if (json.contains("choices") && json["choices"].is_array()
                    && !json["choices"].empty()) {
                    auto choice = json["choices"][0];
                    if (choice.contains("delta") && choice["delta"].contains("content")) {
                        std::string content = choice["delta"]["content"];
                        if (!content.empty()) {
                            callback(content);
                        }
                    }
                }
            } catch (...) {
                // Ignore parse errors in stream chunks (resilience)
            }
        }
    }
}

void LLMEngineAPI::OpenAICompatibleClient::sendRequestStream(
    std::string_view prompt,
    const nlohmann::json& input,
    const nlohmann::json& params,
    std::function<void(std::string_view)> callback) {
    // Build messages array using shared helper
    const nlohmann::json messages = ChatMessageBuilder::buildMessages(prompt, input);

    // Create streaming params
    nlohmann::json stream_params = params;
    stream_params["stream"] = true;

    auto buffer = std::make_shared<std::string>();

    ChatCompletionRequestHelper::executeStream(
        default_params_,
        stream_params,
        // Build payload
        [&](const nlohmann::json& request_params) {
            return buildPayload(messages, request_params);
        },
        // Build URL
        [&]() { return buildUrl(); },
        // Build headers
        [&]() { return getHeaders(); },
        // Stream processor
        [buffer, callback](std::string_view chunk) {
            OpenAICompatibleClient::parseOpenAIStreamChunk(chunk, *buffer, callback);
        },
        config_.get());
}
