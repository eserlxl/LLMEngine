// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "llmengine/providers/api_client.hpp"

#include "llmengine/core/constants.hpp"
#include "llmengine/utils/logger.hpp"
#include "llmengine/providers/provider_bootstrap.hpp"
#include "llmengine/utils/utils.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <ranges>
#include <unordered_map>

namespace LLMEngineAPI {

// APIClientFactory Implementation
std::unique_ptr<APIClient> APIClientFactory::createClient(
    ProviderType type,
    std::string_view apiKey,
    std::string_view model,
    std::string_view baseUrl,
    const std::shared_ptr<IConfigManager>& cfg) {
    switch (type) {
        case ProviderType::qwen: {
            auto ptr = std::make_unique<QwenClient>(std::string(apiKey), std::string(model));
            if (cfg)
                ptr->setConfig(cfg);
            return ptr;
        }
        case ProviderType::openai: {
            auto ptr = std::make_unique<OpenAIClient>(std::string(apiKey), std::string(model));
            if (cfg)
                ptr->setConfig(cfg);
            return ptr;
        }
        case ProviderType::anthropic: {
            auto ptr = std::make_unique<AnthropicClient>(std::string(apiKey), std::string(model));
            if (cfg)
                ptr->setConfig(cfg);
            return ptr;
        }
        case ProviderType::ollama: {
            auto ptr = std::make_unique<OllamaClient>(std::string(baseUrl), std::string(model));
            if (cfg)
                ptr->setConfig(cfg);
            return ptr;
        }
        case ProviderType::gemini: {
            auto ptr = std::make_unique<GeminiClient>(std::string(apiKey), std::string(model));
            if (cfg)
                ptr->setConfig(cfg);
            return ptr;
        }
        default:
            return nullptr;
    }
}

std::unique_ptr<APIClient> APIClientFactory::createClientFromConfig(
    std::string_view providerName,
    const nlohmann::json& config,
    ::LLMEngine::Logger* logger,
    const std::shared_ptr<IConfigManager>& cfg) {
    ProviderType type = stringToProviderType(providerName);

    // Extract values from config
    std::string apiKeyFromConfig =
        config.value(std::string(::LLMEngine::Constants::JsonKeys::apiKey), "");
    std::string modelFromConfig =
        config.value(std::string(::LLMEngine::Constants::JsonKeys::defaultModel), "");
    std::string baseUrlFromConfig =
        config.value(std::string(::LLMEngine::Constants::JsonKeys::baseUrl), "");

    // Use ProviderBootstrap to resolve credentials and config with proper priority
    auto apiKey =
        ::LLMEngine::ProviderBootstrap::resolveApiKey(type, "", apiKeyFromConfig, logger);
    std::string model =
        ::LLMEngine::ProviderBootstrap::resolveModel(type, "", modelFromConfig, logger);
    std::string baseUrl =
        ::LLMEngine::ProviderBootstrap::resolveBaseUrl(type, "", baseUrlFromConfig, logger);

    // SECURITY: Fail fast if no credentials are available for providers that require them
    // This prevents silent failures in headless or hardened deployments
    if (apiKey.empty() && type != ProviderType::ollama) {
        std::string envVarName = ::LLMEngine::ProviderBootstrap::getApiKeyEnvVarName(type);
        std::string errorMsg = "No API key found for provider " + std::string(providerName) + ". "
                                + "Set the " + envVarName
                                + " environment variable or provide it in the config file.";
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Error, errorMsg);
        }
        throw std::runtime_error(errorMsg);
    }

    // Handle Ollama separately (uses baseUrl instead of apiKey)
    if (type == ProviderType::ollama) {
        // baseUrl and model are already resolved with defaults by ProviderBootstrap
        auto ptr = std::make_unique<OllamaClient>(baseUrl, model);
        if (cfg)
            ptr->setConfig(cfg);
        return ptr;
    }

    return createClient(type, apiKey.view(), model, baseUrl, cfg);
}

namespace {
static const std::unordered_map<std::string, ProviderType>& getProviderMap() {
    static const std::unordered_map<std::string, ProviderType> providerMap = {
        {"qwen", ProviderType::qwen},
        {"openai", ProviderType::openai},
        {"anthropic", ProviderType::anthropic},
        {"ollama", ProviderType::ollama},
        {"gemini", ProviderType::gemini}};
    return providerMap;
}
} // namespace

ProviderType APIClientFactory::stringToProviderType(std::string_view providerName) {
    std::string name(providerName);
    std::ranges::transform(
        name, name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    const auto& providerMap = getProviderMap();
    auto it = providerMap.find(name);
    if (it != providerMap.end()) {
        return it->second;
    }

    // SECURITY: Fail fast on unknown providers instead of silently falling back
    // This prevents unexpected provider selection in multi-tenant contexts
    // If an empty or invalid provider is provided, throw an exception rather than defaulting
    if (!name.empty()) {
        throw std::runtime_error(
            "Unknown provider: " + name
            + ". Supported providers: qwen, openai, anthropic, ollama, gemini");
    }

    // Only use default for empty string (explicit empty provider request)
    // This maintains backward compatibility while preventing silent failures
    const auto& cfg = APIConfigManager::getInstance();
    const std::string def = cfg.getDefaultProvider();
    std::string defLower = def;
    std::ranges::transform(defLower, defLower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    auto defIt = providerMap.find(defLower);
    if (defIt != providerMap.end()) {
        return defIt->second;
    }

    // SECURITY: Fail fast on invalid default provider configuration
    // Falling back to Ollama can route workloads to an unintended local service,
    // potentially exposing secrets to the wrong endpoint. Treat this as a configuration error.
    std::string errorMsg = "Invalid default provider configuration: '" + def
                            + "'. Supported providers: qwen, openai, anthropic, ollama, gemini. "
                              "This is a configuration error and must be fixed to prevent "
                              "unintended provider selection.";

    // Log loudly before throwing (use stderr since getLogger() is private)
    std::cerr << "LLMEngine ERROR: " << errorMsg << "\n";

    throw std::runtime_error(errorMsg);
}

std::string APIClientFactory::providerTypeToString(ProviderType type) {
    const auto& providerMap = getProviderMap();
    for (const auto& [name, enum_type] : providerMap) {
        if (enum_type == type) {
            return name;
        }
    }
    return "ollama"; // Default fallback
}

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
constexpr int MIN_TIMEOUT_SECONDS = 1;
constexpr int maxTimeoutSeconds = 3600;
constexpr int MIN_RETRY_ATTEMPTS = 0;
constexpr int MAX_RETRY_ATTEMPTS = 10;
constexpr int MIN_RETRY_DELAY_MS = 0;
constexpr int MAX_RETRY_DELAY_MS = 60000;

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
            if (timeoutVal < MIN_TIMEOUT_SECONDS || timeoutVal > maxTimeoutSeconds) {
                if (logger) {
                    logger->log(
                        ::LLMEngine::LogLevel::Warn,
                        std::string("Config validation warning: timeout_seconds should be between ")
                            + std::to_string(MIN_TIMEOUT_SECONDS) + " and "
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
            if (retriesVal < MIN_RETRY_ATTEMPTS || retriesVal > MAX_RETRY_ATTEMPTS) {
                if (logger) {
                    logger->log(
                        ::LLMEngine::LogLevel::Warn,
                        std::string("Config validation warning: retry_attempts should be between ")
                            + std::to_string(MIN_RETRY_ATTEMPTS) + " and "
                            + std::to_string(MAX_RETRY_ATTEMPTS)
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
            if (delayVal < MIN_RETRY_DELAY_MS || delayVal > MAX_RETRY_DELAY_MS) {
                if (logger) {
                    logger->log(
                        ::LLMEngine::LogLevel::Warn,
                        std::string("Config validation warning: retry_delay_ms should be between ")
                            + std::to_string(MIN_RETRY_DELAY_MS) + " and "
                            + std::to_string(MAX_RETRY_DELAY_MS)
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
