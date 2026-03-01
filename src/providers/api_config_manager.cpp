// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.

#include "llmengine/providers/api_config_manager.hpp"
#include "llmengine/utils/utils.hpp"
#include "llmengine/core/constants.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>

namespace LLMEngineAPI {

// APIConfigManager Implementation
APIConfigManager::APIConfigManager()
    : defaultConfigPath_(std::string(::LLMEngine::Constants::FilePaths::defaultConfigPath)) {}

APIConfigManager& APIConfigManager::getInstance() {
    static APIConfigManager instance;
    return instance;
}

void APIConfigManager::setDefaultConfigPath(std::string_view configPath) {
    // Exclusive lock for writing default path
    std::unique_lock<std::shared_mutex> lock(mutex_);
    defaultConfigPath_ = std::string(configPath);
}

std::string APIConfigManager::getDefaultConfigPath() const {
    // Shared lock for reading default path
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return defaultConfigPath_;
}

void APIConfigManager::setLogger(std::shared_ptr<::LLMEngine::Logger> logger) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (logger) {
        logger_ = logger;
    } else {
        logger_.reset();
    }
}

std::shared_ptr<::LLMEngine::Logger> APIConfigManager::getLogger() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto locked = logger_.lock();
    if (locked) {
        return locked;
    }
    // Fall back to thread-safe DefaultLogger if original logger was destroyed
    static std::shared_ptr<::LLMEngine::DefaultLogger> defaultLogger =
        std::make_shared<::LLMEngine::DefaultLogger>();
    return defaultLogger;
}

namespace {
// Configuration validation constants
constexpr int minTimeoutSeconds = 1;
constexpr int maxTimeoutSeconds = 3600;
constexpr int minRetryAttempts = 0;
constexpr int maxRetryAttempts = 10;
constexpr int minRetryDelayMs = 0;
constexpr int maxRetryDelayMs = 60000;

// Helper function to validate configuration structure
bool validateConfig(const nlohmann::json& config, ::LLMEngine::Logger* logger) {
    // Validate top-level structure
    if (!config.is_object()) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Error,
                        "Config validation failed: root must be a JSON object");
        }
        return false;
    }

    // Validate providers section
    if (config.contains(std::string(::LLMEngine::Constants::JsonKeys::providers))) {
        const auto& providers = config[std::string(::LLMEngine::Constants::JsonKeys::providers)];
        if (!providers.is_object()) {
            if (logger) {
                logger->log(::LLMEngine::LogLevel::Error,
                            "Config validation failed: 'providers' must be an object");
            }
            return false;
        }

        // Validate each provider configuration
        for (const auto& [providerName, providerConfig] : providers.items()) {
            if (!providerConfig.is_object()) {
                if (logger) {
                    logger->log(::LLMEngine::LogLevel::Error,
                                std::string("Config validation failed: provider '") + providerName
                                    + "' must be an object");
                }
                return false;
            }

            // Validate baseUrl if present
            if (providerConfig.contains(std::string(::LLMEngine::Constants::JsonKeys::baseUrl))) {
                const auto& baseUrl =
                    providerConfig[std::string(::LLMEngine::Constants::JsonKeys::baseUrl)];
                if (baseUrl.is_string()) {
                    std::string urlStr = baseUrl.get<std::string>();
                    if (!urlStr.empty() && !::LLMEngine::Utils::validateUrl(urlStr)) {
                        if (logger) {
                            std::string warningMsg = "Config validation warning: provider '";
                            warningMsg += providerName;
                            warningMsg += "' has invalid baseUrl format: ";
                            warningMsg += urlStr;
                            logger->log(::LLMEngine::LogLevel::Warn, warningMsg);
                        }
                        // Warning only, not a fatal error
                    }
                }
            }

            // Validate default_model if present
            if (providerConfig.contains(
                    std::string(::LLMEngine::Constants::JsonKeys::defaultModel))) {
                const auto& model =
                    providerConfig[std::string(::LLMEngine::Constants::JsonKeys::defaultModel)];
                if (model.is_string()) {
                    std::string modelStr = model.get<std::string>();
                    if (!modelStr.empty() && !::LLMEngine::Utils::validateModelName(modelStr)) {
                        if (logger) {
                            std::string warningMsg = "Config validation warning: provider '";
                            warningMsg += providerName;
                            warningMsg += "' has invalid default_model format: ";
                            warningMsg += modelStr;
                            logger->log(::LLMEngine::LogLevel::Warn, warningMsg);
                        }
                        // Warning only, not a fatal error
                    }
                }
            }
        }
    }

    // Validate timeout_seconds if present
    if (config.contains(std::string(::LLMEngine::Constants::JsonKeys::timeoutSeconds))) {
        const auto& timeout =
            config[std::string(::LLMEngine::Constants::JsonKeys::timeoutSeconds)];
        if (timeout.is_number_integer()) {
            int timeoutVal = timeout.get<int>();
            if (timeoutVal < minTimeoutSeconds || timeoutVal > maxTimeoutSeconds) {
                if (logger) {
                    logger->log(
                        ::LLMEngine::LogLevel::Warn,
                        std::string("Config validation warning: timeout_seconds should be between ")
                            + std::to_string(minTimeoutSeconds) + " and "
                            + std::to_string(maxTimeoutSeconds)
                            + ", got: " + std::to_string(timeoutVal));
                }
            }
        }
    }

    // Validate retry_attempts if present
    if (config.contains(std::string(::LLMEngine::Constants::JsonKeys::retryAttempts))) {
        const auto& retries = config[std::string(::LLMEngine::Constants::JsonKeys::retryAttempts)];
        if (retries.is_number_integer()) {
            int retriesVal = retries.get<int>();
            if (retriesVal < minRetryAttempts || retriesVal > maxRetryAttempts) {
                if (logger) {
                    logger->log(
                        ::LLMEngine::LogLevel::Warn,
                        std::string("Config validation warning: retry_attempts should be between ")
                            + std::to_string(minRetryAttempts) + " and "
                            + std::to_string(maxRetryAttempts)
                            + ", got: " + std::to_string(retriesVal));
                }
            }
        }
    }

    // Validate retry_delay_ms if present
    if (config.contains(std::string(::LLMEngine::Constants::JsonKeys::retryDelayMs))) {
        const auto& delay = config[std::string(::LLMEngine::Constants::JsonKeys::retryDelayMs)];
        if (delay.is_number_integer()) {
            int delayVal = delay.get<int>();
            if (delayVal < minRetryDelayMs || delayVal > maxRetryDelayMs) {
                if (logger) {
                    logger->log(
                        ::LLMEngine::LogLevel::Warn,
                        std::string("Config validation warning: retry_delay_ms should be between ")
                            + std::to_string(minRetryDelayMs) + " and "
                            + std::to_string(maxRetryDelayMs)
                            + ", got: " + std::to_string(delayVal));
                }
            }
        }
    }

    return true;
}
} // namespace

bool APIConfigManager::loadConfig(std::string_view configPath) {
    std::string path;
    if (configPath.empty()) {
        // Use stored default path - read it first with shared lock
        {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            path = defaultConfigPath_;
        }
        // Then acquire exclusive lock for writing configuration
    } else {
        path = std::string(configPath);
    }

    // Get logger BEFORE acquiring exclusive lock to avoid deadlock
    // (getLogger() needs a shared lock, which would conflict with exclusive lock)
    auto logger = getLogger();

    // Exclusive lock for writing configuration
    std::unique_lock<std::shared_mutex> lock(mutex_);

    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            logger->log(::LLMEngine::LogLevel::Error,
                        std::string("Could not open config file: ") + path);
            configLoaded_ = false;
            config_ = nlohmann::json{}; // Clear stale configuration
            return false;
        }

        nlohmann::json loadedConfig;
        file >> loadedConfig;

        // Validate configuration before accepting it
        if (!validateConfig(loadedConfig, logger.get())) {
            logger->log(::LLMEngine::LogLevel::Error,
                        "Config validation failed, using empty configuration");
            configLoaded_ = false;
            config_ = nlohmann::json{}; // Clear stale configuration
            return false;
        }

        config_ = std::move(loadedConfig);
        configLoaded_ = true;
        return true;
    } catch (const nlohmann::json::parse_error& e) {
        logger->log(::LLMEngine::LogLevel::Error,
                    std::string("JSON parse error in config file: ") + e.what() + " at position "
                        + std::to_string(e.byte));
        configLoaded_ = false;
        config_ = nlohmann::json{}; // Clear stale configuration
        return false;
    } catch (const std::exception& e) {
        logger->log(::LLMEngine::LogLevel::Error,
                    std::string("Failed to load config: ") + e.what());
        configLoaded_ = false;
        config_ = nlohmann::json{}; // Clear stale configuration
        return false;
    }
}

nlohmann::json APIConfigManager::getProviderConfig(std::string_view providerName) const {
    // Shared lock for reading configuration
    std::shared_lock<std::shared_mutex> lock(mutex_);

    if (!configLoaded_) {
        return nlohmann::json{};
    }

    if (config_.contains(std::string(::LLMEngine::Constants::JsonKeys::providers))) {
        std::string key(providerName);
        if (config_[std::string(::LLMEngine::Constants::JsonKeys::providers)].contains(key)) {
            return config_[std::string(::LLMEngine::Constants::JsonKeys::providers)][key];
        }
    }

    return nlohmann::json{};
}

std::vector<std::string> APIConfigManager::getAvailableProviders() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    std::vector<std::string> providers;

    if (configLoaded_
        && config_.contains(std::string(::LLMEngine::Constants::JsonKeys::providers))) {
        for (auto& [key, value] :
             config_[std::string(::LLMEngine::Constants::JsonKeys::providers)].items()) {
            providers.push_back(key);
        }
    }

    return providers;
}

std::string APIConfigManager::getDefaultProvider() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (!configLoaded_
        || !config_.contains(std::string(::LLMEngine::Constants::JsonKeys::defaultProvider))) {
        return "ollama";
    }
    return config_[std::string(::LLMEngine::Constants::JsonKeys::defaultProvider)]
        .get<std::string>();
}

int APIConfigManager::getTimeoutSeconds() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (!configLoaded_
        || !config_.contains(std::string(::LLMEngine::Constants::JsonKeys::timeoutSeconds))) {
        return ::LLMEngine::Constants::DefaultValues::timeoutSeconds;
    }
    return config_[std::string(::LLMEngine::Constants::JsonKeys::timeoutSeconds)].get<int>();
}

int APIConfigManager::getTimeoutSeconds(std::string_view providerName) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    int defaultTimeout = ::LLMEngine::Constants::DefaultValues::timeoutSeconds;

    if (!configLoaded_) {
        // Provider-specific defaults
        if (providerName == "ollama") {
            return ::LLMEngine::Constants::DefaultValues::ollamaTimeoutSeconds;
        }
        return defaultTimeout;
    }

    // Check global timeout
    if (config_.contains(std::string(::LLMEngine::Constants::JsonKeys::timeoutSeconds))) {
        defaultTimeout =
            config_[std::string(::LLMEngine::Constants::JsonKeys::timeoutSeconds)].get<int>();
    }

    // Check provider-specific timeout
    if (!providerName.empty()
        && config_.contains(std::string(::LLMEngine::Constants::JsonKeys::providers))) {
        std::string providerKey(providerName);
        if (config_[std::string(::LLMEngine::Constants::JsonKeys::providers)].contains(
                providerKey)) {
            auto providerConfig =
                config_[std::string(::LLMEngine::Constants::JsonKeys::providers)][providerKey];
            if (providerConfig.contains(
                    std::string(::LLMEngine::Constants::JsonKeys::timeoutSeconds))) {
                return providerConfig[std::string(
                                           ::LLMEngine::Constants::JsonKeys::timeoutSeconds)]
                    .get<int>();
            }
        }
    }

    // Provider-specific defaults if not in config
    if (providerName == "ollama") {
        return ::LLMEngine::Constants::DefaultValues::ollamaTimeoutSeconds;
    }

    return defaultTimeout;
}

int APIConfigManager::getRetryAttempts() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (!configLoaded_
        || !config_.contains(std::string(::LLMEngine::Constants::JsonKeys::retryAttempts))) {
        return ::LLMEngine::Constants::DefaultValues::retryAttempts;
    }
    return config_[std::string(::LLMEngine::Constants::JsonKeys::retryAttempts)].get<int>();
}

int APIConfigManager::getRetryDelayMs() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (!configLoaded_
        || !config_.contains(std::string(::LLMEngine::Constants::JsonKeys::retryDelayMs))) {
        return ::LLMEngine::Constants::DefaultValues::retryDelayMs;
    }
    return config_[std::string(::LLMEngine::Constants::JsonKeys::retryDelayMs)].get<int>();
}
} // namespace LLMEngineAPI
