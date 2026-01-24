// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/APIClient.hpp"
#include "LLMEngine/ErrorCodes.hpp"
#include "LLMEngine/HttpStatus.hpp"
#include "LLMEngine/IRequestExecutor.hpp"
#include "LLMEngine/IRetryStrategy.hpp"

#include <chrono>
#include <stdexcept>
#include <thread>

namespace LLMEngine {

RetryableRequestExecutor::RetryableRequestExecutor(std::shared_ptr<IRetryStrategy> strategy,
                                                   std::shared_ptr<IRequestExecutor> base_executor)
    : strategy_(std::move(strategy)), base_executor_(std::move(base_executor)) {
    if (!strategy_) {
        throw std::invalid_argument("Retry strategy must not be null");
    }
}

::LLMEngineAPI::APIResponse RetryableRequestExecutor::execute(
    const ::LLMEngineAPI::APIClient* api_client,
    const std::string& full_prompt,
    const nlohmann::json& input,
    const nlohmann::json& final_params,
    const ::LLMEngine::RequestOptions& options) const {
    if (!api_client) {
        ::LLMEngineAPI::APIResponse error_response;
        error_response.success = false;
        error_response.error_message = "API client not initialized";
        error_response.status_code = HttpStatus::INTERNAL_SERVER_ERROR;
        error_response.error_code = LLMEngine::LLMEngineErrorCode::Unknown;
        return error_response;
    }

    ::LLMEngineAPI::APIResponse response;
    const int max_attempts = options.max_retries.value_or(strategy_->getMaxAttempts());

    for (int attempt = 1; attempt <= max_attempts; ++attempt) {
        // Check cancellation
        if (options.cancellation_token && options.cancellation_token->isCancelled()) {
            response.success = false;
            response.error_message = "Request cancelled";
            response.error_code = LLMEngineErrorCode::Cancelled;
            return response;
        }

        // Execute request via base executor or directly
        if (base_executor_) {
            response =
                base_executor_->execute(api_client, full_prompt, input, final_params, options);
        } else {
            response = api_client->sendRequest(full_prompt, input, final_params, options);
        }

        // Check cancellation (post-request)
        if (options.cancellation_token && options.cancellation_token->isCancelled()) {
            response.success = false;
            response.error_message = "Request cancelled";
            response.error_code = LLMEngineErrorCode::Cancelled;
            return response;
        }

        // Check if request succeeded
        const bool is_success = response.success && HttpStatus::isSuccess(response.status_code);
        if (is_success) {
            return response;
        }

        // Determine if we should retry
        const bool is_network_error = response.error_code == LLMEngineErrorCode::Network
                                      || response.error_code == LLMEngineErrorCode::Timeout;
        const int http_status = response.status_code;

        if (!strategy_->shouldRetry(attempt, http_status, is_network_error)) {
            // Don't retry - return the error response
            return response;
        }

        // Get delay before next retry
        if (attempt < max_attempts) {
            const int delay_ms = strategy_->getDelayMs(attempt);
            if (delay_ms > 0) {
                // Check cancellation before sleeping
                if (options.cancellation_token && options.cancellation_token->isCancelled()) {
                    response.success = false;
                    response.error_message = "Request cancelled";
                    response.error_code = LLMEngineErrorCode::Cancelled;
                    return response;
                }

                // Sleep in small chunks to support responsive cancellation?
                // For now just sleep, or check token?
                // Ideally wait on condition variable, but token is simple atomic.
                // Let's just sleep. Better: loop sleep.
                auto start = std::chrono::steady_clock::now();
                auto end = start + std::chrono::milliseconds(delay_ms);
                while (std::chrono::steady_clock::now() < end) {
                    if (options.cancellation_token && options.cancellation_token->isCancelled()) {
                        response.success = false;
                        response.error_message = "Request cancelled";
                        response.error_code = LLMEngineErrorCode::Cancelled;
                        return response;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 10ms poll
                }
            }
        }
    }

    // Max attempts reached
    return response;
}

void RetryableRequestExecutor::executeStream(const ::LLMEngineAPI::APIClient* api_client,
                                             const std::string& full_prompt,
                                             const nlohmann::json& input,
                                             const nlohmann::json& final_params,
                                             LLMEngine::StreamCallback callback,
                                             const ::LLMEngine::RequestOptions& options) const {
    // Note: Streaming retries are complex and not fully implemented yet.
    // We pass through to the base executor or client directly.
    if (base_executor_) {
        base_executor_->executeStream(
            api_client, full_prompt, input, final_params, std::move(callback), options);
    } else if (api_client) {
        api_client->sendRequestStream(
            full_prompt, input, final_params, std::move(callback), options);
    } else {
        throw std::runtime_error("API client not initialized");
    }
}

} // namespace LLMEngine
