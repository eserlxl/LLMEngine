// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include "LLMEngine/LLMEngineExport.hpp"

#include <cstdint>
#include <functional>

namespace LLMEngine {

/**
 * @brief Interface for retry/backoff strategies.
 *
 * This interface allows pluggable retry and backoff policies to be composed
 * into request execution, improving resilience against transient network failures.
 *
 * ## Thread Safety
 *
 * Implementations should be thread-safe if they will be shared across multiple
 * threads or used concurrently. The `shouldRetry()` and `getDelayMs()` methods
 * may be called from multiple threads simultaneously.
 *
 * ## Usage
 *
 * ```cpp
 * class CustomRetryStrategy : public IRetryStrategy {
 *   public:
 *     bool shouldRetry(int attempt, int http_status_code) const override {
 *         return attempt < 3 && http_status_code >= 500;
 *     }
 *     int getDelayMs(int attempt) const override {
 *         return 1000 * attempt; // Linear backoff
 *     }
 * };
 *
 * auto executor = std::make_shared<RetryableRequestExecutor>(
 *     std::make_shared<CustomRetryStrategy>());
 * engine.setRequestExecutor(executor);
 * ```
 */
class LLMENGINE_EXPORT IRetryStrategy {
  public:
    virtual ~IRetryStrategy() = default;

    /**
     * @brief Determine if a request should be retried.
     *
     * @param attempt Current attempt number (1-based, so first attempt is 1)
     * @param http_status_code HTTP status code from the response (0 if no response)
     * @param is_network_error True if the error was a network-level failure (timeout, connection error, etc.)
     * @return True if the request should be retried, false otherwise
     */
    [[nodiscard]] virtual bool shouldRetry(int attempt, int http_status_code, bool is_network_error) const = 0;

    /**
     * @brief Get the delay in milliseconds before the next retry attempt.
     *
     * This method is only called if `shouldRetry()` returned true.
     *
     * @param attempt Current attempt number (1-based)
     * @return Delay in milliseconds before the next retry
     */
    [[nodiscard]] virtual int getDelayMs(int attempt) const = 0;

    /**
     * @brief Get the maximum number of retry attempts.
     *
     * @return Maximum number of attempts (including the initial attempt)
     */
    [[nodiscard]] virtual int getMaxAttempts() const = 0;
};

/**
 * @brief Default retry strategy with exponential backoff.
 *
 * This strategy retries on:
 * - Network errors (timeouts, connection failures)
 * - Server errors (5xx HTTP status codes)
 * - Retriable client errors (429 Too Many Requests, 408 Request Timeout)
 *
 * Uses exponential backoff with configurable base delay and maximum delay.
 */
class LLMENGINE_EXPORT DefaultRetryStrategy : public IRetryStrategy {
  public:
    /**
     * @brief Construct with default settings.
     *
     * Defaults:
     * - max_attempts: 3
     * - base_delay_ms: 1000
     * - max_delay_ms: 30000
     */
    DefaultRetryStrategy() : max_attempts_(3), base_delay_ms_(1000), max_delay_ms_(30000) {}

    /**
     * @brief Construct with custom settings.
     *
     * @param max_attempts Maximum number of attempts (including initial attempt)
     * @param base_delay_ms Base delay in milliseconds for exponential backoff
     * @param max_delay_ms Maximum delay in milliseconds (caps exponential backoff)
     */
    DefaultRetryStrategy(int max_attempts, int base_delay_ms, int max_delay_ms)
        : max_attempts_(max_attempts), base_delay_ms_(base_delay_ms), max_delay_ms_(max_delay_ms) {}

    [[nodiscard]] bool shouldRetry(int attempt, int http_status_code, bool is_network_error) const override {
        if (attempt >= max_attempts_) {
            return false;
        }

        // Always retry on network errors
        if (is_network_error) {
            return true;
        }

        // Retry on server errors (5xx)
        if (http_status_code >= 500 && http_status_code < 600) {
            return true;
        }

        // Retry on specific client errors
        if (http_status_code == 429 || http_status_code == 408) {
            return true;
        }

        return false;
    }

    [[nodiscard]] int getDelayMs(int attempt) const override {
        // Exponential backoff: base_delay * 2^(attempt-1)
        // Cap at max_delay_ms
        int64_t delay = static_cast<int64_t>(base_delay_ms_) << (attempt - 1);
        if (delay > max_delay_ms_) {
            delay = max_delay_ms_;
        }
        return static_cast<int>(delay);
    }

    [[nodiscard]] int getMaxAttempts() const override { return max_attempts_; }

  private:
    int max_attempts_;
    int base_delay_ms_;
    int max_delay_ms_;
};

/**
 * @brief No-op retry strategy that never retries.
 *
 * Useful for testing or when retries should be disabled.
 */
class LLMENGINE_EXPORT NoRetryStrategy : public IRetryStrategy {
  public:
    [[nodiscard]] bool shouldRetry(int, int, bool) const override { return false; }
    [[nodiscard]] int getDelayMs(int) const override { return 0; }
    [[nodiscard]] int getMaxAttempts() const override { return 1; }
};

} // namespace LLMEngine

