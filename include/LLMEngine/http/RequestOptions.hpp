// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "LLMEngine/utils/CancellationToken.hpp"
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace LLMEngine {

/**
 * @brief Per-request options for LLM operations.
 */
struct RequestOptions {
    /**
     * @brief Request timeout in milliseconds.
     * Overrides default configuration if set.
     */
    std::optional<int> timeout_ms;

    /**
     * @brief Maximum number of retries for this request.
     * Overrides default configuration if set.
     */
    std::optional<int> max_retries;

    /**
     * @brief Extra HTTP headers to include in the request.
     */
    std::map<std::string, std::string> extra_headers;

    /**
     * @brief Token for cancelling the operation.
     */
    std::shared_ptr<CancellationToken> cancellation_token;

    /**
     * @brief Maximum concurrency for batch operations.
     * Functions like analyzeBatch will limit parallel requests to this number.
     * Default depends on implementation (e.g., usually unbounded or high limit).
     */
    std::optional<size_t> max_concurrency;

    struct GenerationOptions {
        std::optional<double> temperature;
        std::optional<int> max_tokens;
        std::optional<double> top_p;
        std::optional<double> frequency_penalty;
        std::optional<double> presence_penalty;
        std::vector<std::string> stop_sequences;
        std::optional<int> seed;
        std::optional<nlohmann::json> logit_bias;
        std::optional<bool> logprobs;
        std::optional<int> top_logprobs;
        std::optional<int> top_k;
        std::optional<double> min_p;
        std::optional<std::string> user;
        std::optional<bool> parallel_tool_calls;
        std::optional<std::string> service_tier;
        std::optional<std::string> reasoning_effort;
        std::optional<int> max_completion_tokens;

        // [NEW] Structured Output Support
        std::optional<nlohmann::json> response_format;

        // [NEW] Tool Choice Override
        std::optional<nlohmann::json> tool_choice;
    } generation;

    struct StreamOptions {
        bool include_usage = false;
    };
    std::optional<StreamOptions> stream_options;

    /**
     * @brief Merge two RequestOptions objects.
     * @param defaults The default options (lower priority).
     * @param overrides The overriding options (higher priority).
     * @return A new RequestOptions object with merged values.
     */
    static RequestOptions merge(const RequestOptions& defaults, const RequestOptions& overrides) {
        RequestOptions result = defaults;

        // Override if present in 'overrides'
        if (overrides.timeout_ms) result.timeout_ms = overrides.timeout_ms;
        if (overrides.max_retries) result.max_retries = overrides.max_retries;
        
        // Merge headers
        for (const auto& [key, val] : overrides.extra_headers) {
            result.extra_headers[key] = val;
        }

        if (overrides.cancellation_token) result.cancellation_token = overrides.cancellation_token;
        if (overrides.max_concurrency) result.max_concurrency = overrides.max_concurrency;

        // Generation Options
        if (overrides.generation.temperature) result.generation.temperature = overrides.generation.temperature;
        if (overrides.generation.max_tokens) result.generation.max_tokens = overrides.generation.max_tokens;
        if (overrides.generation.top_p) result.generation.top_p = overrides.generation.top_p;
        if (overrides.generation.frequency_penalty) result.generation.frequency_penalty = overrides.generation.frequency_penalty;
        if (overrides.generation.presence_penalty) result.generation.presence_penalty = overrides.generation.presence_penalty;
        if (!overrides.generation.stop_sequences.empty()) result.generation.stop_sequences = overrides.generation.stop_sequences;
        if (overrides.generation.seed) result.generation.seed = overrides.generation.seed;
        if (overrides.generation.logit_bias) result.generation.logit_bias = overrides.generation.logit_bias;
        if (overrides.generation.logprobs) result.generation.logprobs = overrides.generation.logprobs;
        if (overrides.generation.top_logprobs) result.generation.top_logprobs = overrides.generation.top_logprobs;
        if (overrides.generation.top_k) result.generation.top_k = overrides.generation.top_k;
        if (overrides.generation.min_p) result.generation.min_p = overrides.generation.min_p;
        if (overrides.generation.user) result.generation.user = overrides.generation.user;
        if (overrides.generation.parallel_tool_calls) result.generation.parallel_tool_calls = overrides.generation.parallel_tool_calls;
        if (overrides.generation.service_tier) result.generation.service_tier = overrides.generation.service_tier;
        if (overrides.generation.reasoning_effort) result.generation.reasoning_effort = overrides.generation.reasoning_effort;
        if (overrides.generation.max_completion_tokens) result.generation.max_completion_tokens = overrides.generation.max_completion_tokens;
        if (overrides.generation.response_format) result.generation.response_format = overrides.generation.response_format;
        if (overrides.generation.tool_choice) result.generation.tool_choice = overrides.generation.tool_choice;

        if (overrides.stream_options) result.stream_options = overrides.stream_options;

        return result;
    }
};

/**
 * @brief Builder for RequestOptions.
 */
class RequestOptionsBuilder {
public:
    RequestOptionsBuilder& setTimeout(int timeoutMs) {
        m_options.timeout_ms = timeoutMs;
        return *this;
    }

    RequestOptionsBuilder& setMaxRetries(int maxRetries) {
        m_options.max_retries = maxRetries;
        return *this;
    }

    RequestOptionsBuilder& addHeader(const std::string& key, const std::string& value) {
        m_options.extra_headers[key] = value;
        return *this;
    }

    RequestOptionsBuilder& setCancellationToken(std::shared_ptr<CancellationToken> token) {
        m_options.cancellation_token = std::move(token);
        return *this;
    }

    RequestOptionsBuilder& setMaxConcurrency(size_t maxConcurrency) {
        m_options.max_concurrency = maxConcurrency;
        return *this;
    }

    RequestOptionsBuilder& setTemperature(double temperature) {
        m_options.generation.temperature = temperature;
        return *this;
    }

    RequestOptionsBuilder& setMaxTokens(int maxTokens) {
        m_options.generation.max_tokens = maxTokens;
        return *this;
    }

    RequestOptionsBuilder& setTopP(double topP) {
        m_options.generation.top_p = topP;
        return *this;
    }

    RequestOptionsBuilder& setFrequencyPenalty(double frequencyPenalty) {
        m_options.generation.frequency_penalty = frequencyPenalty;
        return *this;
    }

    RequestOptionsBuilder& setPresencePenalty(double presencePenalty) {
        m_options.generation.presence_penalty = presencePenalty;
        return *this;
    }

    RequestOptionsBuilder& addStopSequence(const std::string& stopSequence) {
        m_options.generation.stop_sequences.push_back(stopSequence);
        return *this;
    }

    RequestOptionsBuilder& setSeed(int seed) {
        m_options.generation.seed = seed;
        return *this;
    }

    RequestOptionsBuilder& setLogitBias(const nlohmann::json& logitBias) {
        m_options.generation.logit_bias = logitBias;
        return *this;
    }

    RequestOptionsBuilder& setLogprobs(bool enable, std::optional<int> topLogprobs = std::nullopt) {
        m_options.generation.logprobs = enable;
        if (topLogprobs.has_value()) {
            m_options.generation.top_logprobs = topLogprobs;
        }
        return *this;
    }

    RequestOptionsBuilder& setTopK(int topK) {
        m_options.generation.top_k = topK;
        return *this;
    }

    RequestOptionsBuilder& setMinP(double minP) {
        m_options.generation.min_p = minP;
        return *this;
    }

    RequestOptionsBuilder& setUser(const std::string& userId) {
        m_options.generation.user = userId;
        return *this;
    }

    RequestOptionsBuilder& setParallelToolCalls(bool enable) {
        m_options.generation.parallel_tool_calls = enable;
        return *this;
    }

    RequestOptionsBuilder& setServiceTier(const std::string& tier) {
        m_options.generation.service_tier = tier;
        return *this;
    }

    RequestOptionsBuilder& setReasoningEffort(const std::string& effort) {
        m_options.generation.reasoning_effort = effort;
        return *this;
    }

    RequestOptionsBuilder& setMaxCompletionTokens(int tokens) {
        m_options.generation.max_completion_tokens = tokens;
        return *this;
    }

    RequestOptionsBuilder& setResponseFormat(const nlohmann::json& format) {
        m_options.generation.response_format = format;
        return *this;
    }

    RequestOptionsBuilder& setToolChoice(const nlohmann::json& choice) {
        m_options.generation.tool_choice = choice;
        return *this;
    }

    RequestOptionsBuilder& setStreamOptions(bool includeUsage) {
        m_options.stream_options = RequestOptions::StreamOptions{includeUsage};
        return *this;
    }

    RequestOptions build() const {
        return m_options;
    }

private:
    RequestOptions m_options;
};

} // namespace LLMEngine
