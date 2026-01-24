// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include "LLMEngine/ErrorCodes.hpp"
#include "LLMEngine/LLMEngineExport.hpp"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

namespace LLMEngine {

// Backward compatibility alias - use LLMEngineErrorCode in new code
using AnalysisErrorCode = LLMEngineErrorCode;

/**
 * @brief Result structure returned by LLMEngine::analyze().
 *
 * Contains the success status, extracted content, and error information.
 */
struct LLMENGINE_EXPORT AnalysisResult {
    bool success;
    std::string think;
    std::string content;
    std::string finishReason;
    std::string errorMessage;
    int statusCode;

    /**
     * @brief Token usage statistics.
     */
    struct UsageStats {
        int promptTokens = 0;
        int completionTokens = 0;
        int totalTokens = 0;
        // Extended usage stats
        int reasoning_tokens = 0;
        int cached_tokens = 0;
    } usage;

    struct TokenLogProb {
        std::string token;
        double logprob;
        std::optional<std::vector<uint8_t>> bytes; // Raw bytes if useful
    };
    std::optional<std::vector<TokenLogProb>> logprobs;

    /**
     * @brief Structured error code for programmatic error handling.
     *
     * Use this instead of parsing errorMessage for error classification.
     * Only valid when success == false.
     */
    LLMEngineErrorCode errorCode = LLMEngineErrorCode::None;

    /**
     * @brief Check if the result represents a specific error type.
     */
    [[nodiscard]] bool hasError(AnalysisErrorCode code) const {
        return !success && errorCode == code;
    }

    /**
     * @brief Check if the result is a retriable error (network, timeout, server, rate limit).
     */
    [[nodiscard]] bool isRetriableError() const {
        return !success
               && (errorCode == LLMEngineErrorCode::Network
                   || errorCode == LLMEngineErrorCode::Timeout
                   || errorCode == LLMEngineErrorCode::Server
                   || errorCode == LLMEngineErrorCode::RateLimited);
    }

    struct ToolCall {
        std::string id;
        std::string name;
        std::string arguments;
    };
    std::vector<ToolCall> tool_calls;

    [[nodiscard]] bool hasToolCalls() const {
        return !tool_calls.empty();
    }
};

/**
 * @brief Payload for streaming callbacks.
 */
struct StreamChunk {
    std::string_view content;
    bool is_done;
    LLMEngineErrorCode error_code = LLMEngineErrorCode::None;
    std::string error_message;
    std::optional<AnalysisResult::UsageStats> usage;
    std::string finish_reason;
    std::optional<std::vector<AnalysisResult::TokenLogProb>> logprobs;
};

using StreamCallback = std::function<void(const StreamChunk&)>;

} // namespace LLMEngine
