// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "open_ai_compatible_client.hpp"

#include <string>

namespace LLMEngineAPI {

namespace {
constexpr size_t chatCompletionsPathLength = 18; // "/chat/completions" = 18 chars
constexpr size_t bearerPrefixLength = 7;          // "Bearer " = 7 chars
} // namespace

OpenAICompatibleClient::OpenAICompatibleClient(const std::string& apiKey,
                                               const std::string& model,
                                               const std::string& baseUrl)
    : apiKey_(apiKey), model_(model), baseUrl_(baseUrl) {
    // Initialize default parameters
    defaultParams_ = {{"temperature", ::LLMEngine::Constants::DefaultValues::temperature},
                       {"max_tokens", ::LLMEngine::Constants::DefaultValues::maxTokens},
                       {"top_p", ::LLMEngine::Constants::DefaultValues::topP},
                       {"frequency_penalty", 0.0},
                       {"presence_penalty", 0.0}};

    // Cache the chat completions URL to avoid string concatenation on every request
    chatCompletionsUrl_.reserve(baseUrl_.size() + chatCompletionsPathLength);
    chatCompletionsUrl_ = baseUrl_;
    chatCompletionsUrl_ += "/chat/completions";

    // Cache headers to avoid rebuilding on every request
    std::string authHeader;
    authHeader.reserve(apiKey_.size() + bearerPrefixLength);
    authHeader = "Bearer ";
    authHeader += apiKey_;

    cachedHeaders_ = {{"Content-Type", "application/json"},
                       {"Authorization", std::move(authHeader)}};
}

const std::map<std::string, std::string>& OpenAICompatibleClient::getHeaders() const {
    return cachedHeaders_; // Return cached headers
}

std::string OpenAICompatibleClient::buildUrl() const {
    return chatCompletionsUrl_; // Return cached URL
}

nlohmann::json OpenAICompatibleClient::buildPayload(const nlohmann::json& messages,
                                                    const nlohmann::json& requestParams) const {
    nlohmann::json payload = {{"model", model_},
                              {"messages", messages},
                              {"temperature", requestParams["temperature"]},
                              {"max_tokens", requestParams["max_tokens"]},
                              {"top_p", requestParams["top_p"]},
                              {"frequency_penalty", requestParams["frequency_penalty"]},
                              {"presence_penalty", requestParams["presence_penalty"]}};

    // Optional params
    if (requestParams.contains("seed")) {
        payload["seed"] = requestParams["seed"];
    }
    if (requestParams.contains("parallel_tool_calls")) {
        payload["parallel_tool_calls"] = requestParams["parallel_tool_calls"];
    }
    if (requestParams.contains("service_tier")) {
        payload["service_tier"] = requestParams["service_tier"];
    }
    if (requestParams.contains("stream_options")) {
        payload["stream_options"] = requestParams["stream_options"];
    }

    // Include response_format if present (Structured Outputs)
    if (requestParams.contains("response_format")) {
        payload["response_format"] = requestParams["response_format"];
    }
    if (requestParams.contains("reasoning_effort")) {
        payload["reasoning_effort"] = requestParams["reasoning_effort"];
    }
    if (requestParams.contains("max_completion_tokens")) {
        payload["max_completion_tokens"] = requestParams["max_completion_tokens"];
    }

    // Include tool definitions if present
    if (requestParams.contains("tools")) {
        payload["tools"] = requestParams["tools"];
    }
    if (requestParams.contains("tool_choice")) {
        payload["tool_choice"] = requestParams["tool_choice"];
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
    if (response.rawResponse.contains("choices") && response.rawResponse["choices"].is_array()
        && !response.rawResponse["choices"].empty()) {

        auto choice = response.rawResponse["choices"][0];
        if (choice.contains("message") && choice["message"].contains("content")) {
            response.content = choice["message"]["content"].get<std::string>();
            response.success = true;

            if (choice.contains("finishReason") && !choice["finishReason"].is_null()) {
                response.finishReason = choice["finishReason"].get<std::string>();
            }

            // Parse token usage if available
            if (response.rawResponse.contains("usage")
                && response.rawResponse["usage"].is_object()) {
                const auto& usage = response.rawResponse["usage"];
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
            // response.errorMessage = "No content in response";
            response.errorMessage = "No content in response";
            response.errorCode = LLMEngine::LLMEngineErrorCode::InvalidResponse;
        }
    } else {
        // response.errorMessage = "Invalid response format";
        response.errorMessage = "Invalid response format";
        response.errorCode = LLMEngine::LLMEngineErrorCode::InvalidResponse;
    }
}
} // namespace LLMEngineAPI

// Note: LLMEngine::StreamCallback is used here
void LLMEngineAPI::OpenAICompatibleClient::parseOpenAIStreamChunk(
    std::string_view chunk, std::string& buffer, const LLMEngine::StreamCallback& callback) {
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
                result.isDone = true;
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
                        result.isDone = false;
                        if (choice.contains("logprobs") && !choice["logprobs"].is_null()) {
                             const auto& logprobsObj = choice["logprobs"];
                             if (logprobsObj.contains("content") && logprobsObj["content"].is_array()) {
                                 std::vector<LLMEngine::AnalysisResult::TokenLogProb> chunkLogprobs;
                                 for (const auto& item : logprobsObj["content"]) {
                                     LLMEngine::AnalysisResult::TokenLogProb lp;
                                     if (item.contains("token")) lp.token = item["token"].get<std::string>();
                                     if (item.contains("logprob")) lp.logprob = item["logprob"].get<double>();
                                     if (item.contains("bytes") && item["bytes"].is_array()) {
                                         lp.bytes = item["bytes"].get<std::vector<uint8_t>>();
                                     }
                                     chunkLogprobs.push_back(std::move(lp));
                                 }
                                 if (!chunkLogprobs.empty()) {
                                     result.logprobs = std::move(chunkLogprobs);
                                 }
                             }
                        }
                        callback(result);
                    }
                    if (choice.contains("finishReason") && !choice["finishReason"].is_null()) {
                        std::string finishReason = choice["finishReason"].get<std::string>();
                        if (!finishReason.empty()) {
                             LLMEngine::StreamChunk result;
                             result.isDone = false; // Will trigger done on [DONE] or next event
                             result.finishReason = finishReason;
                             // Some providers send empty delta with finishReason
                             callback(result);
                        }
                    }
                }

                if (json.contains("usage") && !json["usage"].is_null()) {
                    LLMEngine::StreamChunk result;
                    result.isDone = false; // Usage might come before DONE or with DONE
                    result.usage = LLMEngine::AnalysisResult::UsageStats{
                        .promptTokens = json["usage"].value("prompt_tokens", 0),
                        .completionTokens = json["usage"].value("completion_tokens", 0),
                        .totalTokens = json["usage"].value("total_tokens", 0)};
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
    const LLMEngine::StreamCallback& callback,
    const ::LLMEngine::RequestOptions& options) {
    // Build messages array using shared helper
    const nlohmann::json messages = ChatMessageBuilder::buildMessages(prompt, input);
    auto buffer = std::make_shared<std::string>();

    ChatCompletionRequestHelper::executeStream(
        defaultParams_,
        params,
        // Build payload
        [&](const nlohmann::json& requestParams) {
            nlohmann::json payload = buildPayload(messages, requestParams);
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
