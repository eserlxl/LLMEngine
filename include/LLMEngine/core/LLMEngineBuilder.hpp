// Copyright © 2026 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "LLMEngine/core/LLMEngine.hpp"
#include "LLMEngine/LLMEngineExport.hpp"
#include <chrono>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>

namespace LLMEngine {

/**
 * @brief Fluent builder for constructing LLMEngine instances.
 *
 * Simplifies construction by replacing multi-argument constructors
 * with named methods and sensible defaults.
 */
class LLMENGINE_EXPORT LLMEngineBuilder {
  public:
    LLMEngineBuilder() = default;

    LLMEngineBuilder& withProvider(std::string_view name);
    LLMEngineBuilder& withApiKey(std::string_view key);
    LLMEngineBuilder& withModel(std::string_view model);
    LLMEngineBuilder& withModelParams(const nlohmann::json& params);
    LLMEngineBuilder& withConfigManager(std::shared_ptr<::LLMEngineAPI::IConfigManager> cfg);
    LLMEngineBuilder& withLogger(std::shared_ptr<Logger> logger);
    LLMEngineBuilder& enableDebug(bool enabled = true);
    LLMEngineBuilder& withLogRetention(int hours);

    LLMEngineBuilder& withBaseUrl(std::string_view url);
    
    // New Configuration Methods
    LLMEngineBuilder& withTimeout(std::chrono::milliseconds timeout);
    LLMEngineBuilder& withMaxRetries(int retries);

    /**
     * @brief Build the LLMEngine instance.
     * @throws std::runtime_error if configuration is invalid.
     */
    [[nodiscard]] std::unique_ptr<LLMEngine> build();

  private:
    std::string provider_name_;
    std::string api_key_;
    std::string model_;
    std::string base_url_;
    nlohmann::json model_params_;
    std::shared_ptr<::LLMEngineAPI::IConfigManager> config_manager_;
    std::shared_ptr<Logger> logger_;
    bool debug_ = false;
    int log_retention_hours_ = 24;
    
    // Default options
    std::optional<int> timeout_ms_;
    std::optional<int> max_retries_;
};

} // namespace LLMEngine
