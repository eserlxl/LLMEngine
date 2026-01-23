// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include "LLMEngine/LLMEngineExport.hpp"

#include "LLMEngine/RequestOptions.hpp"
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

namespace LLMEngineAPI {
class APIClient;
struct APIResponse;
} // namespace LLMEngineAPI

namespace LLMEngine {

/**
 * @brief Interface for executing provider requests.
 */
class LLMENGINE_EXPORT IRequestExecutor {
  public:
    virtual ~IRequestExecutor() = default;

    /**
     * @brief Execute a request using the provided API client.
     */
    [[nodiscard]] virtual ::LLMEngineAPI::APIResponse execute(
        const ::LLMEngineAPI::APIClient* api_client,
        const std::string& full_prompt,
        const nlohmann::json& input,
        const nlohmann::json& final_params,
        const ::LLMEngine::RequestOptions& options = {}) const = 0;
};

/**
 * @brief Default request executor that directly calls APIClient::sendRequest.
 */
class LLMENGINE_EXPORT DefaultRequestExecutor : public IRequestExecutor {
  public:
    [[nodiscard]] ::LLMEngineAPI::APIResponse execute(
        const ::LLMEngineAPI::APIClient* api_client,
        const std::string& full_prompt,
        const nlohmann::json& input,
        const nlohmann::json& final_params,
        const ::LLMEngine::RequestOptions& options = {}) const override;
};

// Forward declaration
class IRetryStrategy;

/**
 * @brief Request executor that applies retry/backoff strategy.
 *
 * This executor wraps another executor (or directly calls APIClient) and applies
 * retry logic based on an injected IRetryStrategy. See IRetryStrategy for details
 * on retry policies.
 *
 * ## Usage
 *
 * ```cpp
 * #include "LLMEngine/IRetryStrategy.hpp"
 * #include "LLMEngine/IRequestExecutor.hpp"
 *
 * auto strategy = std::make_shared<DefaultRetryStrategy>(3, 1000, 30000);
 * auto executor = std::make_shared<RetryableRequestExecutor>(strategy);
 * engine.setRequestExecutor(executor);
 * ```
 */
class LLMENGINE_EXPORT RetryableRequestExecutor : public IRequestExecutor {
  public:
    /**
     * @brief Construct with a retry strategy.
     *
     * @param strategy Retry strategy to use (shared ownership)
     * @param base_executor Optional base executor. If nullptr, directly calls APIClient.
     *                      If provided, retries are applied to the base executor's results.
     */
    RetryableRequestExecutor(std::shared_ptr<IRetryStrategy> strategy,
                             std::shared_ptr<IRequestExecutor> base_executor = nullptr);

    [[nodiscard]] ::LLMEngineAPI::APIResponse execute(
        const ::LLMEngineAPI::APIClient* api_client,
        const std::string& full_prompt,
        const nlohmann::json& input,
        const nlohmann::json& final_params,
        const ::LLMEngine::RequestOptions& options = {}) const override;

  private:
    std::shared_ptr<IRetryStrategy> strategy_;
    std::shared_ptr<IRequestExecutor> base_executor_;
};

} // namespace LLMEngine
