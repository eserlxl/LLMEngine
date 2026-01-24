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
    nlohmann::json payload = {{"model", model_},
                              {"messages", messages},
                              {"temperature", request_params["temperature"]},
                              {"max_tokens", request_params["max_tokens"]},
                              {"top_p", request_params["top_p"]},
                              {"frequency_penalty", request_params["frequency_penalty"]},
                              {"presence_penalty", request_params["presence_penalty"]}};

    // Include response_format if present (Structured Outputs)
    if (request_params.contains("response_format")) {
        payload["response_format"] = request_params["response_format"];
    }

    // Include tool definitions if present
    if (request_params.contains("tools")) {
        payload["tools"] = request_params["tools"];
    }
    if (request_params.contains("tool_choice")) {
        payload["tool_choice"] = request_params["tool_choice"];
    }

    // Include extra fields passed via AnalysisInput::withExtraField
    // Note: AnalysisInput::toJson merges extra_fields into the root of the JSON
    // We iterate specific known keys to avoid polluting payload with internal engine params,
    // but we should ideally support dynamic extra params.
    // For now, response_format is the critical addition.

    return payload;
}

void OpenAICompatibleClient::parseOpenAIResponse(APIResponse& response,
                                                 const std::string& /*unused*/) {
    if (response.raw_response.contains("choices") && response.raw_response["choices"].is_array()
        && !response.raw_response["choices"].empty()) {

        auto choice = response.raw_response["choices"][0];
        if (choice.contains("message") && choice["message"].contains("content")) {
            response.content = choice["message"]["content"].get<std::string>();
            response.success = true;

            if (choice.contains("finish_reason") && !choice["finish_reason"].is_null()) {
                response.finish_reason = choice["finish_reason"].get<std::string>();
            }

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

// Note: LLMEngine::StreamCallback is used here
void LLMEngineAPI::OpenAICompatibleClient::parseOpenAIStreamChunk(
    std::string_view chunk, std::string& buffer, LLMEngine::StreamCallback callback) {
    buffer.append(chunk);

    size_t pos = 0;
    while ((pos = buffer.find("\n")) != std::string::npos) {
        std::string line = buffer.substr(0, pos);
        buffer.erase(0, pos + 1);

        if (line.empty() || line == "\r")
            continue;

        if (line.rfind("data: ", 0) == 0) {
            std::string data = line.substr(6);

            if (data == "[DONE]") {
                LLMEngine::StreamChunk result;
                result.content = "";
                result.is_done = true;
                callback(result);
                continue;
            }

            try {
                auto json = nlohmann::json::parse(data);
                if (json.contains("choices") && json["choices"].is_array()
                    && !json["choices"].empty()) {
                    auto& choice = json["choices"][0];
                    if (choice.contains("delta") && choice["delta"].contains("content")) {
                        std::string content = choice["delta"]["content"].get<std::string>();
                        LLMEngine::StreamChunk result;
                        result.content = content;
                        result.is_done = false;
                        callback(result);
                    }
                    if (choice.contains("finish_reason") && !choice["finish_reason"].is_null()) {
                        // Finish reason handling
                    }
                }

                if (json.contains("usage") && !json["usage"].is_null()) {
                    LLMEngine::StreamChunk result;
                    result.is_done = false; // Usage might come before DONE or with DONE
                    result.usage = LLMEngine::AnalysisResult::UsageStats{
                        .promptTokens = json["usage"].value("prompt_tokens", 0),
                        .completionTokens = json["usage"].value("completion_tokens", 0)};
                    callback(result);
                }

            } catch (const nlohmann::json::parse_error&) {
                // Ignore
            }
        }
    }
}

void LLMEngineAPI::OpenAICompatibleClient::sendRequestStream(
    std::string_view prompt,
    const nlohmann::json& input,
    const nlohmann::json& params,
    LLMEngine::StreamCallback callback,
    const ::LLMEngine::RequestOptions& options) {
    // Build messages array using shared helper
    const nlohmann::json messages = ChatMessageBuilder::buildMessages(prompt, input);
    auto buffer = std::make_shared<std::string>();

    ChatCompletionRequestHelper::executeStream(
        default_params_,
        params,
        // Build payload
        [&](const nlohmann::json& request_params) {
            nlohmann::json payload = buildPayload(messages, request_params);
            payload["stream"] = true; // Force streaming
                                      // Default to include usage for OpenAI compatible
            if (!payload.contains("stream_options")) {
                payload["stream_options"] = {{"include_usage", true}};
            }
            return payload;
        },
        // Build URL
        [&]() { return buildUrl(); },
        // Build headers
        [&]() { return getHeaders(); },
        // Stream processor
        [buffer, callback](std::string_view chunk) {
            OpenAICompatibleClient::parseOpenAIStreamChunk(chunk, *buffer, callback);
        },
        options,
        /*exponential_retry=*/true,
        config_.get());
}
