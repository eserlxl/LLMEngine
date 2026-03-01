// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.

#pragma once

#include "llmengine/LLMEngineExport.hpp"
#include "llmengine/core/i_config_manager.hpp"
#include "llmengine/utils/logger.hpp"

#include <nlohmann/json.hpp>

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

namespace LLMEngineAPI {

/**
 * @brief Singleton managing `api_config.json` loading and access.
 *
 * Implements IConfigManager interface for dependency injection support while
 * maintaining singleton pattern for backward compatibility.
 *
 * ## Thread Safety
 *
 * **Thread-safe.** Uses `std::shared_mutex` to allow concurrent reads and exclusive writes.
 * It is safe to call `loadConfig()` and `getTimeoutSeconds()` from different threads concurrently.
 *
 * ## Usage
 *
 * ### Direct Singleton Access (Deprecated pattern)
 * ```cpp
 * auto& config = APIConfigManager::getInstance();
 * config.loadConfig("api_config.json");
 * int timeout = config.getTimeoutSeconds();
 * ```
 *
 * ### Dependency Injection (Recommended pattern)
 * ```cpp
 * std::shared_ptr<IConfigManager> config = std::make_shared<APIConfigManager>();
 * // or use singleton as shared_ptr
 * auto config = std::shared_ptr<IConfigManager>(&APIConfigManager::getInstance(), [](auto*) {});
 * ```
 */
class LLMENGINE_EXPORT APIConfigManager : public IConfigManager {
  public:
    static APIConfigManager& getInstance();

    // @brief Set the default configuration file path.
    // @param configPath The path to use as default when loadConfig() is called without arguments.
    void setDefaultConfigPath(std::string_view configPath) override;

    // @brief Get the current default configuration file path.
    // @return The current default config path.
    std::string getDefaultConfigPath() const override;

    void setLogger(std::shared_ptr<::LLMEngine::Logger> logger) override;
    bool loadConfig(std::string_view configPath = "") override;
    nlohmann::json getProviderConfig(std::string_view providerName) const override;
    std::vector<std::string> getAvailableProviders() const override;
    std::string getDefaultProvider() const override;
    int getTimeoutSeconds() const override;
    int getTimeoutSeconds(std::string_view providerName) const override;
    int getRetryAttempts() const override;
    int getRetryDelayMs() const override;

  private:
    APIConfigManager();
    mutable std::shared_mutex mutex_; // For read-write lock
    nlohmann::json config_;
    bool configLoaded_ = false;
    std::string defaultConfigPath_; // Default config file path
    std::weak_ptr<::LLMEngine::Logger>
        logger_; // Optional logger (weak ownership to avoid dangling pointers)

    // Helper method to safely get logger (locks weak_ptr or returns DefaultLogger)
    std::shared_ptr<::LLMEngine::Logger> getLogger() const;
};

} // namespace LLMEngineAPI
