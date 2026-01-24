// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "LLMEngine/CancellationToken.hpp"
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
    } generation;

    struct StreamOptions {
        bool include_usage = false;
    };
    std::optional<StreamOptions> stream_options;
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
