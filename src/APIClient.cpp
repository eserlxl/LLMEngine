// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/APIClient.hpp"
#include "LLMEngine/Logger.hpp"
#include "LLMEngine/Constants.hpp"
#include <fstream>
#include <algorithm>
#include <cctype>
#include <cstring>

namespace LLMEngineAPI {

// APIClientFactory Implementation
std::unique_ptr<APIClient> APIClientFactory::createClient(ProviderType type, 
                                                         std::string_view api_key,
                                                         std::string_view model,
                                                         std::string_view base_url) {
    switch (type) {
        case ProviderType::QWEN:
            return std::make_unique<QwenClient>(std::string(api_key), std::string(model));
        case ProviderType::OPENAI:
            return std::make_unique<OpenAIClient>(std::string(api_key), std::string(model));
        case ProviderType::ANTHROPIC:
            return std::make_unique<AnthropicClient>(std::string(api_key), std::string(model));
        case ProviderType::OLLAMA:
            return std::make_unique<OllamaClient>(std::string(base_url), std::string(model));
        case ProviderType::GEMINI:
            return std::make_unique<GeminiClient>(std::string(api_key), std::string(model));
        default:
            return nullptr;
    }
}

std::unique_ptr<APIClient> APIClientFactory::createClientFromConfig(std::string_view provider_name,
                                                                     const nlohmann::json& config,
                                                                     ::LLMEngine::Logger* logger) {
    ProviderType type = stringToProviderType(provider_name);
    if (type == ProviderType::OLLAMA) {
        std::string base_url = config.value(std::string(::LLMEngine::Constants::JsonKeys::BASE_URL), std::string(::LLMEngine::Constants::DefaultUrls::OLLAMA_BASE));
        std::string model = config.value(std::string(::LLMEngine::Constants::JsonKeys::DEFAULT_MODEL), std::string(::LLMEngine::Constants::DefaultModels::OLLAMA));
        return std::make_unique<OllamaClient>(base_url, model);
    }
    
    // SECURITY: Prefer environment variables for API keys over config file
    std::string api_key = config.value(std::string(::LLMEngine::Constants::JsonKeys::API_KEY), "");
    std::string env_var_name;
    switch (type) {
        case ProviderType::QWEN: env_var_name = std::string(::LLMEngine::Constants::EnvVars::QWEN_API_KEY); break;
        case ProviderType::OPENAI: env_var_name = std::string(::LLMEngine::Constants::EnvVars::OPENAI_API_KEY); break;
        case ProviderType::ANTHROPIC: env_var_name = std::string(::LLMEngine::Constants::EnvVars::ANTHROPIC_API_KEY); break;
        case ProviderType::GEMINI: env_var_name = std::string(::LLMEngine::Constants::EnvVars::GEMINI_API_KEY); break;
        default: break;
    }
    
    // Override config API key with environment variable if available
    // SECURITY: Guard against calling std::getenv("") which is undefined behavior
    const char* env_api_key = nullptr;
    if (!env_var_name.empty()) {
        env_api_key = std::getenv(env_var_name.c_str());
    }
    if (env_api_key && std::strlen(env_api_key) > 0) {
        api_key = env_api_key;
    } else if (!api_key.empty()) {
        // Warn if falling back to config file for credentials
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Warn, 
                std::string("Using API key from config file. For production use, ")
                + "set the " + env_var_name + " environment variable instead. "
                + "Storing credentials in config files is a security risk.");
        }
    }
    
    // SECURITY: Fail fast if no credentials are available for providers that require them
    // This prevents silent failures in headless or hardened deployments
    if (api_key.empty() && type != ProviderType::OLLAMA) {
        std::string error_msg = "No API key found for provider " + std::string(provider_name) + ". "
                               + "Set the " + env_var_name + " environment variable or provide it in the config file.";
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Error, error_msg);
        }
        throw std::runtime_error(error_msg);
    }
    
    std::string model = config.value(std::string(::LLMEngine::Constants::JsonKeys::DEFAULT_MODEL), "");
    return createClient(type, api_key, model);
}

ProviderType APIClientFactory::stringToProviderType(std::string_view provider_name) {
    std::string name(provider_name);
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    
    // Direct mapping for known providers
    if (name == "qwen") return ProviderType::QWEN;
    if (name == "openai") return ProviderType::OPENAI;
    if (name == "anthropic") return ProviderType::ANTHROPIC;
    if (name == "ollama") return ProviderType::OLLAMA;
    if (name == "gemini") return ProviderType::GEMINI;
    
    // SECURITY: Fail fast on unknown providers instead of silently falling back
    // This prevents unexpected provider selection in multi-tenant contexts
    // If an empty or invalid provider is provided, throw an exception rather than defaulting
    if (!name.empty()) {
        throw std::runtime_error("Unknown provider: " + name + 
                                 ". Supported providers: qwen, openai, anthropic, ollama, gemini");
    }
    
    // Only use default for empty string (explicit empty provider request)
    // This maintains backward compatibility while preventing silent failures
    const auto& cfg = APIConfigManager::getInstance();
    const std::string def = cfg.getDefaultProvider();
    std::string def_lower = def;
    std::transform(def_lower.begin(), def_lower.end(), def_lower.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    
    if (def_lower == "qwen") return ProviderType::QWEN;
    if (def_lower == "openai") return ProviderType::OPENAI;
    if (def_lower == "anthropic") return ProviderType::ANTHROPIC;
    if (def_lower == "ollama") return ProviderType::OLLAMA;
    if (def_lower == "gemini") return ProviderType::GEMINI;
    
    // Last resort: use Ollama as safe default only if default provider is also invalid
    return ProviderType::OLLAMA;
}

std::string APIClientFactory::providerTypeToString(ProviderType type) {
    switch (type) {
        case ProviderType::QWEN: return "qwen";
        case ProviderType::OPENAI: return "openai";
        case ProviderType::ANTHROPIC: return "anthropic";
        case ProviderType::OLLAMA: return "ollama";
        case ProviderType::GEMINI: return "gemini";
        default: return "ollama";
    }
}

// APIConfigManager Implementation
APIConfigManager::APIConfigManager() : default_config_path_(std::string(::LLMEngine::Constants::FilePaths::DEFAULT_CONFIG_PATH)) {
}

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
    static std::shared_ptr<::LLMEngine::DefaultLogger> default_logger = std::make_shared<::LLMEngine::DefaultLogger>();
    return default_logger;
}

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
    
    // Exclusive lock for writing configuration
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            getLogger()->log(::LLMEngine::LogLevel::Error, std::string("Could not open config file: ") + path);
            config_loaded_ = false;
            config_ = nlohmann::json{}; // Clear stale configuration
            return false;
        }
        
        file >> config_;
        config_loaded_ = true;
        return true;
    } catch (const std::exception& e) {
        getLogger()->log(::LLMEngine::LogLevel::Error, std::string("Failed to load config: ") + e.what());
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
    
    if (config_loaded_ && config_.contains(std::string(::LLMEngine::Constants::JsonKeys::PROVIDERS))) {
        for (auto& [key, value] : config_[std::string(::LLMEngine::Constants::JsonKeys::PROVIDERS)].items()) {
            providers.push_back(key);
        }
    }
    
    return providers;
}

std::string APIConfigManager::getDefaultProvider() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (!config_loaded_ || !config_.contains(std::string(::LLMEngine::Constants::JsonKeys::DEFAULT_PROVIDER))) {
        return "ollama";
    }
    return config_[std::string(::LLMEngine::Constants::JsonKeys::DEFAULT_PROVIDER)].get<std::string>();
}

int APIConfigManager::getTimeoutSeconds() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (!config_loaded_ || !config_.contains(std::string(::LLMEngine::Constants::JsonKeys::TIMEOUT_SECONDS))) {
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
        default_timeout = config_[std::string(::LLMEngine::Constants::JsonKeys::TIMEOUT_SECONDS)].get<int>();
    }
    
    // Check provider-specific timeout
    if (!provider_name.empty() && config_.contains(std::string(::LLMEngine::Constants::JsonKeys::PROVIDERS))) {
        std::string provider_key(provider_name);
        if (config_[std::string(::LLMEngine::Constants::JsonKeys::PROVIDERS)].contains(provider_key)) {
            auto provider_config = config_[std::string(::LLMEngine::Constants::JsonKeys::PROVIDERS)][provider_key];
            if (provider_config.contains(std::string(::LLMEngine::Constants::JsonKeys::TIMEOUT_SECONDS))) {
                return provider_config[std::string(::LLMEngine::Constants::JsonKeys::TIMEOUT_SECONDS)].get<int>();
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
    if (!config_loaded_ || !config_.contains(std::string(::LLMEngine::Constants::JsonKeys::RETRY_ATTEMPTS))) {
        return ::LLMEngine::Constants::DefaultValues::RETRY_ATTEMPTS;
    }
    return config_[std::string(::LLMEngine::Constants::JsonKeys::RETRY_ATTEMPTS)].get<int>();
}

int APIConfigManager::getRetryDelayMs() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (!config_loaded_ || !config_.contains(std::string(::LLMEngine::Constants::JsonKeys::RETRY_DELAY_MS))) {
        return ::LLMEngine::Constants::DefaultValues::RETRY_DELAY_MS;
    }
    return config_[std::string(::LLMEngine::Constants::JsonKeys::RETRY_DELAY_MS)].get<int>();
}

} // namespace LLMEngineAPI
