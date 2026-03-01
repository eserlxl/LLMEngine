// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/providers/ProviderBootstrap.hpp"

#include "LLMEngine/providers/APIClient.hpp"
#include "LLMEngine/core/Constants.hpp"
#include "LLMEngine/core/IConfigManager.hpp"

#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace LLMEngine {

ProviderBootstrap::BootstrapResult ProviderBootstrap::bootstrap(
    std::string_view providerName,
    std::string_view apiKey,
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
            [](::LLMEngineAPI::IConfigManager*) {});
    }

    // Load config
    if (!config_mgr->loadConfig()) {
        if (logger) {
            logger->log(LogLevel::Warn, "Could not load API config, using defaults");
        }
    }

    // Determine provider name (use default if empty)
    std::string resolved_provider(providerName);
    if (resolved_provider.empty()) {
        resolved_provider = config_mgr->getDefaultProvider();
        if (resolved_provider.empty()) {
            resolved_provider = "ollama";
        }
    }

    // Get provider configuration
    auto providerConfig = config_mgr->getProviderConfig(resolved_provider);
    if (providerConfig.empty()) {
        std::string errorMsg =
            std::string("Provider ") + resolved_provider + " not found in config";
        if (logger) {
            logger->log(LogLevel::Error, errorMsg);
        }
        throw std::runtime_error("Invalid provider name");
    }

    // Set provider type
    result.providerType =
        ::LLMEngineAPI::APIClientFactory::stringToProviderType(resolved_provider);

    // Resolve API key with priority: env var → param → config
    std::string apiKeyFromConfig =
        providerConfig.value(std::string(Constants::JsonKeys::API_KEY), "");
    result.apiKey = resolveApiKey(result.providerType, apiKey, apiKeyFromConfig, logger);

    // Set model
    std::string modelFromConfig =
        providerConfig.value(std::string(Constants::JsonKeys::DEFAULT_MODEL), "");
    result.model = resolveModel(result.providerType, model, modelFromConfig, logger);

    // Set Ollama URL if using Ollama (or generic Base URL)
    // For now, only Ollama and OpenAI-likes really use BaseURL configuration dynamically
    std::string baseUrlFromConfig =
        providerConfig.value(std::string(Constants::JsonKeys::BASE_URL), "");

    // Always resolve base URL (e.g. for OpenAI compatible endpoints or Ollama)
    result.ollamaUrl = resolveBaseUrl(result.providerType, "", baseUrlFromConfig, logger);

    return result;
}

SecureString ProviderBootstrap::resolveApiKey(::LLMEngineAPI::ProviderType provider_type,
                                              std::string_view api_key_from_param,
                                              std::string_view apiKeyFromConfig,
                                              Logger* logger) {

    std::string envVarName = getApiKeyEnvVarName(provider_type);

    // SECURITY: Prefer environment variables for API keys over config file or constructor parameter
    // Only use provided apiKey if environment variable is not set
    const char* env_api_key = nullptr;
    if (!envVarName.empty()) {
        env_api_key = std::getenv(envVarName.c_str());
    }

    if (env_api_key && strlen(env_api_key) > 0) {
        return SecureString(env_api_key);
    }
    if (!std::string(api_key_from_param).empty()) {
        // Use provided API key if environment variable is not set
        return SecureString(api_key_from_param);
    }
    // Fall back to config file (last resort - not recommended for production)
    std::string apiKey = std::string(apiKeyFromConfig);
    if (!apiKey.empty() && logger) {
        logger->log(LogLevel::Warn,
                    std::string("Using API key from config file. For production use, ") + "set the "
                        + envVarName + " environment variable instead. "
                        + "Storing credentials in config files is a security risk.");
    }
    return SecureString(std::move(apiKey));
}

std::string ProviderBootstrap::resolveBaseUrl(::LLMEngineAPI::ProviderType provider_type,
                                              std::string_view base_url_from_param,
                                              std::string_view baseUrlFromConfig,
                                              Logger* /*logger*/) {
    std::string envVarName = getBaseUrlEnvVarName(provider_type);
    const char* env_val = nullptr;
    if (!envVarName.empty()) {
        env_val = std::getenv(envVarName.c_str());
    }

    // 1. Env Var
    if (env_val && strlen(env_val) > 0) {
        return std::string(env_val);
    }
    // 2. Param
    if (!base_url_from_param.empty()) {
        return std::string(base_url_from_param);
    }
    // 3. Config
    if (!baseUrlFromConfig.empty()) {
        return std::string(baseUrlFromConfig);
    }

    // 4. Default
    switch (provider_type) {
        case ::LLMEngineAPI::ProviderType::OLLAMA:
            return std::string(Constants::DefaultUrls::OLLAMA_BASE);
        case ::LLMEngineAPI::ProviderType::QWEN:
            return std::string(Constants::DefaultUrls::QWEN_BASE);
        case ::LLMEngineAPI::ProviderType::OPENAI:
            return std::string(Constants::DefaultUrls::OPENAI_BASE);
        case ::LLMEngineAPI::ProviderType::ANTHROPIC:
            return std::string(Constants::DefaultUrls::ANTHROPIC_BASE);
        case ::LLMEngineAPI::ProviderType::GEMINI:
            return std::string(Constants::DefaultUrls::GEMINI_BASE);
        default:
            return "";
    }
}

std::string ProviderBootstrap::resolveModel(::LLMEngineAPI::ProviderType provider_type,
                                            std::string_view model_from_param,
                                            std::string_view modelFromConfig,
                                            Logger* /*logger*/) {
    std::string envVarName = getModelEnvVarName(provider_type);
    const char* env_val = nullptr;
    if (!envVarName.empty()) {
        env_val = std::getenv(envVarName.c_str());
    }

    // 1. Provider-specific Env Var
    if (env_val && strlen(env_val) > 0) {
        return std::string(env_val);
    }

    // 1b. Global Default Env Var (DEFAULT_MODEL)
    const std::string env_var = std::string(Constants::EnvVars::DEFAULT_MODEL);
    const char* global_env = std::getenv(env_var.c_str());
    if (global_env && strlen(global_env) > 0) {
        return std::string(global_env);
    }

    // 2. Param
    if (!model_from_param.empty()) {
        return std::string(model_from_param);
    }
    // 3. Config
    if (!modelFromConfig.empty()) {
        return std::string(modelFromConfig);
    }

    // 4. Default
    switch (provider_type) {
        case ::LLMEngineAPI::ProviderType::QWEN:
            return std::string(Constants::DefaultModels::QWEN);
        case ::LLMEngineAPI::ProviderType::OPENAI:
            return std::string(Constants::DefaultModels::OPENAI);
        case ::LLMEngineAPI::ProviderType::ANTHROPIC:
            return std::string(Constants::DefaultModels::ANTHROPIC);
        case ::LLMEngineAPI::ProviderType::GEMINI:
            return std::string(Constants::DefaultModels::GEMINI);
        case ::LLMEngineAPI::ProviderType::OLLAMA:
            return std::string(Constants::DefaultModels::OLLAMA);
        default:
            return "";
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
            return ""; // Ollama doesn't use API keys
    }
}

std::string ProviderBootstrap::getBaseUrlEnvVarName(::LLMEngineAPI::ProviderType provider_type) {
    switch (provider_type) {
        case ::LLMEngineAPI::ProviderType::OLLAMA:
            return std::string(Constants::EnvVars::OLLAMA_HOST);
        case ::LLMEngineAPI::ProviderType::OPENAI:
            return std::string(Constants::EnvVars::OPENAI_BASE_URL);
        case ::LLMEngineAPI::ProviderType::QWEN:
            return std::string(Constants::EnvVars::QWEN_BASE_URL);
        case ::LLMEngineAPI::ProviderType::ANTHROPIC:
            return std::string(Constants::EnvVars::ANTHROPIC_BASE_URL);
        case ::LLMEngineAPI::ProviderType::GEMINI:
            return std::string(Constants::EnvVars::GEMINI_BASE_URL);
        default:
            return "";
    }
}

std::string ProviderBootstrap::getModelEnvVarName(::LLMEngineAPI::ProviderType provider_type) {
    switch (provider_type) {
        case ::LLMEngineAPI::ProviderType::OLLAMA:
            return std::string(Constants::EnvVars::OLLAMA_MODEL);
        case ::LLMEngineAPI::ProviderType::OPENAI:
            return std::string(Constants::EnvVars::OPENAI_MODEL);
        case ::LLMEngineAPI::ProviderType::QWEN:
            return std::string(Constants::EnvVars::QWEN_MODEL);
        case ::LLMEngineAPI::ProviderType::ANTHROPIC:
            return std::string(Constants::EnvVars::ANTHROPIC_MODEL);
        case ::LLMEngineAPI::ProviderType::GEMINI:
            return std::string(Constants::EnvVars::GEMINI_MODEL);
        default:
            return "";
    }
}

} // namespace LLMEngine
