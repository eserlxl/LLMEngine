// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/ProviderBootstrap.hpp"
#include "LLMEngine/APIClient.hpp"
#include "LLMEngine/Constants.hpp"
#include "LLMEngine/IConfigManager.hpp"
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace LLMEngine {

ProviderBootstrap::BootstrapResult ProviderBootstrap::bootstrap(
    std::string_view provider_name,
    std::string_view api_key,
    std::string_view model,
    const std::shared_ptr<::LLMEngineAPI::IConfigManager>& config_manager,
    Logger* logger) {
    
    BootstrapResult result;
    
    // Use injected config manager or fall back to singleton
    std::shared_ptr<::LLMEngineAPI::IConfigManager> config_mgr = config_manager;
    if (!config_mgr) {
        // Wrap singleton in shared_ptr with no-op deleter for compatibility
        config_mgr = std::shared_ptr<::LLMEngineAPI::IConfigManager>(
            &::LLMEngineAPI::APIConfigManager::getInstance(),
            [](::LLMEngineAPI::IConfigManager*){});
    }
    
    // Load config
    if (!config_mgr->loadConfig()) {
        if (logger) {
            logger->log(LogLevel::Warn, "Could not load API config, using defaults");
        }
    }
    
    // Determine provider name (use default if empty)
    std::string resolved_provider(provider_name);
    if (resolved_provider.empty()) {
        resolved_provider = config_mgr->getDefaultProvider();
        if (resolved_provider.empty()) {
            resolved_provider = "ollama";
        }
    }
    
    // Get provider configuration
    auto provider_config = config_mgr->getProviderConfig(resolved_provider);
    if (provider_config.empty()) {
        std::string error_msg = std::string("Provider ") + resolved_provider + " not found in config";
        if (logger) {
            logger->log(LogLevel::Error, error_msg);
        }
        throw std::runtime_error("Invalid provider name");
    }
    
    // Set provider type
    result.provider_type = ::LLMEngineAPI::APIClientFactory::stringToProviderType(resolved_provider);
    
    // Resolve API key with priority: env var → param → config
    std::string api_key_from_config = provider_config.value(
        std::string(Constants::JsonKeys::API_KEY), "");
    result.api_key = resolveApiKey(result.provider_type, api_key, api_key_from_config, logger);
    
    // Set model
    if (std::string(model).empty()) {
        result.model = provider_config.value(
            std::string(Constants::JsonKeys::DEFAULT_MODEL), "");
    } else {
        result.model = std::string(model);
    }
    
    // Set Ollama URL if using Ollama
    if (result.provider_type == ::LLMEngineAPI::ProviderType::OLLAMA) {
        result.ollama_url = provider_config.value(
            std::string(Constants::JsonKeys::BASE_URL),
            std::string(Constants::DefaultUrls::OLLAMA_BASE));
    }
    
    return result;
}

std::string ProviderBootstrap::resolveApiKey(
    ::LLMEngineAPI::ProviderType provider_type,
    std::string_view api_key_from_param,
    std::string_view api_key_from_config,
    Logger* logger) {
    
    std::string env_var_name = getApiKeyEnvVarName(provider_type);
    
    // SECURITY: Prefer environment variables for API keys over config file or constructor parameter
    // Only use provided api_key if environment variable is not set
    const char* env_api_key = nullptr;
    if (!env_var_name.empty()) {
        env_api_key = std::getenv(env_var_name.c_str());
    }
    
    if (env_api_key && strlen(env_api_key) > 0) {
        return std::string(env_api_key);
    } else if (!std::string(api_key_from_param).empty()) {
        // Use provided API key if environment variable is not set
        return std::string(api_key_from_param);
    } else {
        // Fall back to config file (last resort - not recommended for production)
        std::string api_key = std::string(api_key_from_config);
        if (!api_key.empty() && logger) {
            logger->log(LogLevel::Warn, std::string("Using API key from config file. For production use, ")
                      + "set the " + env_var_name + " environment variable instead. "
                      + "Storing credentials in config files is a security risk.");
        }
        return api_key;
    }
}

std::string ProviderBootstrap::getApiKeyEnvVarName(::LLMEngineAPI::ProviderType provider_type) {
    switch (provider_type) {
        case ::LLMEngineAPI::ProviderType::QWEN:
            return std::string(Constants::EnvVars::QWEN_API_KEY);
        case ::LLMEngineAPI::ProviderType::OPENAI:
            return std::string(Constants::EnvVars::OPENAI_API_KEY);
        case ::LLMEngineAPI::ProviderType::ANTHROPIC:
            return std::string(Constants::EnvVars::ANTHROPIC_API_KEY);
        case ::LLMEngineAPI::ProviderType::GEMINI:
            return std::string(Constants::EnvVars::GEMINI_API_KEY);
        default:
            return "";  // Ollama and others don't use API keys
    }
}

} // namespace LLMEngine

