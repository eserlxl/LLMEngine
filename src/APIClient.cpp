// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/APIClient.hpp"

#include "LLMEngine/Constants.hpp"
#include "LLMEngine/Logger.hpp"
#include "LLMEngine/ProviderBootstrap.hpp"
#include "LLMEngine/Utils.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <ranges>

namespace LLMEngineAPI {

// APIClientFactory Implementation
std::unique_ptr<APIClient> APIClientFactory::createClient(
    ProviderType type,
    std::string_view api_key,
    std::string_view model,
    std::string_view base_url,
    const std::shared_ptr<IConfigManager>& cfg) {
    switch (type) {
        case ProviderType::QWEN: {
            auto ptr = std::make_unique<QwenClient>(std::string(api_key), std::string(model));
            if (cfg)
                ptr->setConfig(cfg);
            return ptr;
        }
        case ProviderType::OPENAI: {
            auto ptr = std::make_unique<OpenAIClient>(std::string(api_key), std::string(model));
            if (cfg)
                ptr->setConfig(cfg);
            return ptr;
        }
        case ProviderType::ANTHROPIC: {
            auto ptr = std::make_unique<AnthropicClient>(std::string(api_key), std::string(model));
            if (cfg)
                ptr->setConfig(cfg);
            return ptr;
        }
        case ProviderType::OLLAMA: {
            auto ptr = std::make_unique<OllamaClient>(std::string(base_url), std::string(model));
            if (cfg)
                ptr->setConfig(cfg);
            return ptr;
        }
        case ProviderType::GEMINI: {
            auto ptr = std::make_unique<GeminiClient>(std::string(api_key), std::string(model));
            if (cfg)
                ptr->setConfig(cfg);
            return ptr;
        }
        default:
            return nullptr;
    }
}

std::unique_ptr<APIClient> APIClientFactory::createClientFromConfig(
    std::string_view provider_name,
    const nlohmann::json& config,
    ::LLMEngine::Logger* logger,
    const std::shared_ptr<IConfigManager>& cfg) {
    ProviderType type = stringToProviderType(provider_name);

    // Extract values from config
    std::string api_key_from_config =
        config.value(std::string(::LLMEngine::Constants::JsonKeys::API_KEY), "");
    std::string model =
        config.value(std::string(::LLMEngine::Constants::JsonKeys::DEFAULT_MODEL), "");

    // Use ProviderBootstrap to resolve API key with proper priority
    std::string api_key =
        ::LLMEngine::ProviderBootstrap::resolveApiKey(type, "", api_key_from_config, logger);

    // SECURITY: Fail fast if no credentials are available for providers that require them
    // This prevents silent failures in headless or hardened deployments
    if (api_key.empty() && type != ProviderType::OLLAMA) {
        std::string env_var_name = ::LLMEngine::ProviderBootstrap::getApiKeyEnvVarName(type);
        std::string error_msg = "No API key found for provider " + std::string(provider_name) + ". "
                                + "Set the " + env_var_name
                                + " environment variable or provide it in the config file.";
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Error, error_msg);
        }
        throw std::runtime_error(error_msg);
    }

    // Handle Ollama separately (uses base_url instead of api_key)
    if (type == ProviderType::OLLAMA) {
        std::string base_url =
            config.value(std::string(::LLMEngine::Constants::JsonKeys::BASE_URL),
                         std::string(::LLMEngine::Constants::DefaultUrls::OLLAMA_BASE));
        if (model.empty()) {
            model = std::string(::LLMEngine::Constants::DefaultModels::OLLAMA);
        }
        auto ptr = std::make_unique<OllamaClient>(base_url, model);
        if (cfg)
            ptr->setConfig(cfg);
        return ptr;
    }

    return createClient(type, api_key, model, "", cfg);
}

ProviderType APIClientFactory::stringToProviderType(std::string_view provider_name) {
    std::string name(provider_name);
    std::ranges::transform(
        name, name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    // Direct mapping for known providers
    if (name == "qwen")
        return ProviderType::QWEN;
    if (name == "openai")
        return ProviderType::OPENAI;
    if (name == "anthropic")
        return ProviderType::ANTHROPIC;
    if (name == "ollama")
        return ProviderType::OLLAMA;
    if (name == "gemini")
        return ProviderType::GEMINI;

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
    std::string def_lower = def;
    std::ranges::transform(def_lower, def_lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (def_lower == "qwen")
        return ProviderType::QWEN;
    if (def_lower == "openai")
        return ProviderType::OPENAI;
    if (def_lower == "anthropic")
        return ProviderType::ANTHROPIC;
    if (def_lower == "ollama")
        return ProviderType::OLLAMA;
    if (def_lower == "gemini")
        return ProviderType::GEMINI;

    // SECURITY: Fail fast on invalid default provider configuration
    // Falling back to Ollama can route workloads to an unintended local service,
    // potentially exposing secrets to the wrong endpoint. Treat this as a configuration error.
    std::string error_msg = "Invalid default provider configuration: '" + def
                            + "'. Supported providers: qwen, openai, anthropic, ollama, gemini. "
                              "This is a configuration error and must be fixed to prevent "
                              "unintended provider selection.";

    // Log loudly before throwing (use stderr since getLogger() is private)
    std::cerr << "LLMEngine ERROR: " << error_msg << std::endl;

    throw std::runtime_error(error_msg);
}

std::string APIClientFactory::providerTypeToString(ProviderType type) {
    switch (type) {
        case ProviderType::QWEN:
            return "qwen";
        case ProviderType::OPENAI:
            return "openai";
        case ProviderType::ANTHROPIC:
            return "anthropic";
        case ProviderType::OLLAMA:
            return "ollama";
        case ProviderType::GEMINI:
            return "gemini";
        default:
            return "ollama";
    }
}

// APIConfigManager Implementation
APIConfigManager::APIConfigManager()
    : default_config_path_(std::string(::LLMEngine::Constants::FilePaths::DEFAULT_CONFIG_PATH)) {}

APIConfigManager& APIConfigManager::getInstance() {
    static APIConfigManager instance;
    return instance;
}

void APIConfigManager::setDefaultConfigPath(std::string_view config_path) {
    // Exclusive lock for writing default path
    std::unique_lock<std::shared_mutex> lock(mutex_);
    default_config_path_ = std::string(config_path);
}

std::string APIConfigManager::getDefaultConfigPath() const {
    // Shared lock for reading default path
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return default_config_path_;
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
    static std::shared_ptr<::LLMEngine::DefaultLogger> default_logger =
        std::make_shared<::LLMEngine::DefaultLogger>();
    return default_logger;
}

namespace {
// Configuration validation constants
constexpr int MIN_TIMEOUT_SECONDS = 1;
constexpr int MAX_TIMEOUT_SECONDS = 3600;
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
    if (config.contains(std::string(::LLMEngine::Constants::JsonKeys::PROVIDERS))) {
        const auto& providers = config[std::string(::LLMEngine::Constants::JsonKeys::PROVIDERS)];
        if (!providers.is_object()) {
            if (logger) {
                logger->log(::LLMEngine::LogLevel::Error,
                            "Config validation failed: 'providers' must be an object");
            }
            return false;
        }

        // Validate each provider configuration
        for (const auto& [provider_name, provider_config] : providers.items()) {
            if (!provider_config.is_object()) {
                if (logger) {
                    logger->log(::LLMEngine::LogLevel::Error,
                                std::string("Config validation failed: provider '") + provider_name
                                    + "' must be an object");
                }
                return false;
            }

            // Validate base_url if present
            if (provider_config.contains(std::string(::LLMEngine::Constants::JsonKeys::BASE_URL))) {
                const auto& base_url =
                    provider_config[std::string(::LLMEngine::Constants::JsonKeys::BASE_URL)];
                if (base_url.is_string()) {
                    std::string url_str = base_url.get<std::string>();
                    if (!url_str.empty() && !::LLMEngine::Utils::validateUrl(url_str)) {
                        if (logger) {
                            std::string warning_msg = "Config validation warning: provider '";
                            warning_msg += provider_name;
                            warning_msg += "' has invalid base_url format: ";
                            warning_msg += url_str;
                            logger->log(::LLMEngine::LogLevel::Warn, warning_msg);
                        }
                        // Warning only, not a fatal error
                    }
                }
            }

            // Validate default_model if present
            if (provider_config.contains(
                    std::string(::LLMEngine::Constants::JsonKeys::DEFAULT_MODEL))) {
                const auto& model =
                    provider_config[std::string(::LLMEngine::Constants::JsonKeys::DEFAULT_MODEL)];
                if (model.is_string()) {
                    std::string model_str = model.get<std::string>();
                    if (!model_str.empty() && !::LLMEngine::Utils::validateModelName(model_str)) {
                        if (logger) {
                            std::string warning_msg = "Config validation warning: provider '";
                            warning_msg += provider_name;
                            warning_msg += "' has invalid default_model format: ";
                            warning_msg += model_str;
                            logger->log(::LLMEngine::LogLevel::Warn, warning_msg);
                        }
                        // Warning only, not a fatal error
                    }
                }
            }
        }
    }

    // Validate timeout_seconds if present
    if (config.contains(std::string(::LLMEngine::Constants::JsonKeys::TIMEOUT_SECONDS))) {
        const auto& timeout =
            config[std::string(::LLMEngine::Constants::JsonKeys::TIMEOUT_SECONDS)];
        if (timeout.is_number_integer()) {
            int timeout_val = timeout.get<int>();
            if (timeout_val < MIN_TIMEOUT_SECONDS || timeout_val > MAX_TIMEOUT_SECONDS) {
                if (logger) {
                    logger->log(
                        ::LLMEngine::LogLevel::Warn,
                        std::string("Config validation warning: timeout_seconds should be between ")
                            + std::to_string(MIN_TIMEOUT_SECONDS) + " and "
                            + std::to_string(MAX_TIMEOUT_SECONDS)
                            + ", got: " + std::to_string(timeout_val));
                }
            }
        }
    }

    // Validate retry_attempts if present
    if (config.contains(std::string(::LLMEngine::Constants::JsonKeys::RETRY_ATTEMPTS))) {
        const auto& retries = config[std::string(::LLMEngine::Constants::JsonKeys::RETRY_ATTEMPTS)];
        if (retries.is_number_integer()) {
            int retries_val = retries.get<int>();
            if (retries_val < MIN_RETRY_ATTEMPTS || retries_val > MAX_RETRY_ATTEMPTS) {
                if (logger) {
                    logger->log(
                        ::LLMEngine::LogLevel::Warn,
                        std::string("Config validation warning: retry_attempts should be between ")
                            + std::to_string(MIN_RETRY_ATTEMPTS) + " and "
                            + std::to_string(MAX_RETRY_ATTEMPTS)
                            + ", got: " + std::to_string(retries_val));
                }
            }
        }
    }

    // Validate retry_delay_ms if present
    if (config.contains(std::string(::LLMEngine::Constants::JsonKeys::RETRY_DELAY_MS))) {
        const auto& delay = config[std::string(::LLMEngine::Constants::JsonKeys::RETRY_DELAY_MS)];
        if (delay.is_number_integer()) {
            int delay_val = delay.get<int>();
            if (delay_val < MIN_RETRY_DELAY_MS || delay_val > MAX_RETRY_DELAY_MS) {
                if (logger) {
                    logger->log(
                        ::LLMEngine::LogLevel::Warn,
                        std::string("Config validation warning: retry_delay_ms should be between ")
                            + std::to_string(MIN_RETRY_DELAY_MS) + " and "
                            + std::to_string(MAX_RETRY_DELAY_MS)
                            + ", got: " + std::to_string(delay_val));
                }
            }
        }
    }

    return true;
}
} // namespace

bool APIConfigManager::loadConfig(std::string_view config_path) {
    std::string path;
    if (config_path.empty()) {
        // Use stored default path - read it first with shared lock
        {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            path = default_config_path_;
        }
        // Then acquire exclusive lock for writing configuration
    } else {
        path = std::string(config_path);
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
            config_loaded_ = false;
            config_ = nlohmann::json{}; // Clear stale configuration
            return false;
        }

        nlohmann::json loaded_config;
        file >> loaded_config;

        // Validate configuration before accepting it
        if (!validateConfig(loaded_config, logger.get())) {
            logger->log(::LLMEngine::LogLevel::Error,
                        "Config validation failed, using empty configuration");
            config_loaded_ = false;
            config_ = nlohmann::json{}; // Clear stale configuration
            return false;
        }

        config_ = std::move(loaded_config);
        config_loaded_ = true;
        return true;
    } catch (const nlohmann::json::parse_error& e) {
        logger->log(::LLMEngine::LogLevel::Error,
                    std::string("JSON parse error in config file: ") + e.what() + " at position "
                        + std::to_string(e.byte));
        config_loaded_ = false;
        config_ = nlohmann::json{}; // Clear stale configuration
        return false;
    } catch (const std::exception& e) {
        logger->log(::LLMEngine::LogLevel::Error,
                    std::string("Failed to load config: ") + e.what());
        config_loaded_ = false;
        config_ = nlohmann::json{}; // Clear stale configuration
        return false;
    }
}

nlohmann::json APIConfigManager::getProviderConfig(std::string_view provider_name) const {
    // Shared lock for reading configuration
    std::shared_lock<std::shared_mutex> lock(mutex_);

    if (!config_loaded_) {
        return nlohmann::json{};
    }

    if (config_.contains(std::string(::LLMEngine::Constants::JsonKeys::PROVIDERS))) {
        std::string key(provider_name);
        if (config_[std::string(::LLMEngine::Constants::JsonKeys::PROVIDERS)].contains(key)) {
            return config_[std::string(::LLMEngine::Constants::JsonKeys::PROVIDERS)][key];
        }
    }

    return nlohmann::json{};
}

std::vector<std::string> APIConfigManager::getAvailableProviders() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    std::vector<std::string> providers;

    if (config_loaded_
        && config_.contains(std::string(::LLMEngine::Constants::JsonKeys::PROVIDERS))) {
        for (auto& [key, value] :
             config_[std::string(::LLMEngine::Constants::JsonKeys::PROVIDERS)].items()) {
            providers.push_back(key);
        }
    }

    return providers;
}

std::string APIConfigManager::getDefaultProvider() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (!config_loaded_
        || !config_.contains(std::string(::LLMEngine::Constants::JsonKeys::DEFAULT_PROVIDER))) {
        return "ollama";
    }
    return config_[std::string(::LLMEngine::Constants::JsonKeys::DEFAULT_PROVIDER)]
        .get<std::string>();
}

int APIConfigManager::getTimeoutSeconds() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (!config_loaded_
        || !config_.contains(std::string(::LLMEngine::Constants::JsonKeys::TIMEOUT_SECONDS))) {
        return ::LLMEngine::Constants::DefaultValues::TIMEOUT_SECONDS;
    }
    return config_[std::string(::LLMEngine::Constants::JsonKeys::TIMEOUT_SECONDS)].get<int>();
}

int APIConfigManager::getTimeoutSeconds(std::string_view provider_name) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    int default_timeout = ::LLMEngine::Constants::DefaultValues::TIMEOUT_SECONDS;

    if (!config_loaded_) {
        // Provider-specific defaults
        if (provider_name == "ollama") {
            return ::LLMEngine::Constants::DefaultValues::OLLAMA_TIMEOUT_SECONDS;
        }
        return default_timeout;
    }

    // Check global timeout
    if (config_.contains(std::string(::LLMEngine::Constants::JsonKeys::TIMEOUT_SECONDS))) {
        default_timeout =
            config_[std::string(::LLMEngine::Constants::JsonKeys::TIMEOUT_SECONDS)].get<int>();
    }

    // Check provider-specific timeout
    if (!provider_name.empty()
        && config_.contains(std::string(::LLMEngine::Constants::JsonKeys::PROVIDERS))) {
        std::string provider_key(provider_name);
        if (config_[std::string(::LLMEngine::Constants::JsonKeys::PROVIDERS)].contains(
                provider_key)) {
            auto provider_config =
                config_[std::string(::LLMEngine::Constants::JsonKeys::PROVIDERS)][provider_key];
            if (provider_config.contains(
                    std::string(::LLMEngine::Constants::JsonKeys::TIMEOUT_SECONDS))) {
                return provider_config[std::string(
                                           ::LLMEngine::Constants::JsonKeys::TIMEOUT_SECONDS)]
                    .get<int>();
            }
        }
    }

    // Provider-specific defaults if not in config
    if (provider_name == "ollama") {
        return ::LLMEngine::Constants::DefaultValues::OLLAMA_TIMEOUT_SECONDS;
    }

    return default_timeout;
}

int APIConfigManager::getRetryAttempts() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (!config_loaded_
        || !config_.contains(std::string(::LLMEngine::Constants::JsonKeys::RETRY_ATTEMPTS))) {
        return ::LLMEngine::Constants::DefaultValues::RETRY_ATTEMPTS;
    }
    return config_[std::string(::LLMEngine::Constants::JsonKeys::RETRY_ATTEMPTS)].get<int>();
}

int APIConfigManager::getRetryDelayMs() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (!config_loaded_
        || !config_.contains(std::string(::LLMEngine::Constants::JsonKeys::RETRY_DELAY_MS))) {
        return ::LLMEngine::Constants::DefaultValues::RETRY_DELAY_MS;
    }
    return config_[std::string(::LLMEngine::Constants::JsonKeys::RETRY_DELAY_MS)].get<int>();
}

} // namespace LLMEngineAPI
