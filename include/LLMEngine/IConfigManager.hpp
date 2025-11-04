// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>
#include "LLMEngine/LLMEngineExport.hpp"

// Forward declaration
namespace LLMEngine {
    struct Logger;
}

namespace LLMEngineAPI {

/**
 * @brief Interface for configuration management.
 * 
 * This interface allows dependency injection of configuration management,
 * enabling better testability and flexibility. The default implementation
 * is APIConfigManager, which provides a singleton-based configuration system.
 * 
 * ## Thread Safety
 * 
 * **All methods must be thread-safe** and can be called concurrently from
 * multiple threads.
 * 
 * ## Ownership
 * 
 * Implementations are typically owned via shared_ptr or unique_ptr, depending
 * on the use case. The interface does not prescribe ownership semantics.
 */
class LLMENGINE_EXPORT IConfigManager {
public:
    virtual ~IConfigManager() = default;
    
    /**
     * @brief Set the default configuration file path.
     */
    virtual void setDefaultConfigPath(std::string_view config_path) = 0;
    
    /**
     * @brief Get the current default configuration file path.
     */
    [[nodiscard]] virtual std::string getDefaultConfigPath() const = 0;
    
    /**
     * @brief Set logger for error messages (optional).
     * 
     * @param logger Logger instance (shared ownership, can be nullptr to clear)
     */
    virtual void setLogger(std::shared_ptr<::LLMEngine::Logger> logger) = 0;
    
    /**
     * @brief Load configuration from path or default search order.
     * 
     * @param config_path Optional explicit path. If empty, uses the default path.
     * @return true if configuration loaded successfully.
     */
    virtual bool loadConfig(std::string_view config_path = "") = 0;
    
    /**
     * @brief Get provider-specific JSON config.
     */
    [[nodiscard]] virtual nlohmann::json getProviderConfig(std::string_view provider_name) const = 0;
    
    /**
     * @brief List all available provider keys.
     */
    [[nodiscard]] virtual std::vector<std::string> getAvailableProviders() const = 0;
    
    /**
     * @brief Get default provider key.
     */
    [[nodiscard]] virtual std::string getDefaultProvider() const = 0;
    
    /**
     * @brief Get global timeout seconds.
     */
    [[nodiscard]] virtual int getTimeoutSeconds() const = 0;
    
    /**
     * @brief Get timeout seconds for a specific provider.
     * 
     * Falls back to global timeout if not provider-specific.
     */
    [[nodiscard]] virtual int getTimeoutSeconds(std::string_view provider_name) const = 0;
    
    /**
     * @brief Get global retry attempts.
     */
    [[nodiscard]] virtual int getRetryAttempts() const = 0;
    
    /**
     * @brief Get delay in milliseconds between retries.
     */
    [[nodiscard]] virtual int getRetryDelayMs() const = 0;
};

} // namespace LLMEngineAPI

