// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "llmengine/providers/provider_bootstrap.hpp"

#include "llmengine/providers/api_client.hpp"
#include "llmengine/core/constants.hpp"
#include "llmengine/core/i_config_manager.hpp"

#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace LLMEngine {

ProviderBootstrap::BootstrapResult ProviderBootstrap::bootstrap(
    std::string_view providerName,
    std::string_view apiKey,
    std::string_view model,
    const std::shared_ptr<::LLMEngineAPI::IConfigManager>& configManager,
    Logger* logger) {

    BootstrapResult result;

    // Use injected config manager or fall back to singleton
    std::shared_ptr<::LLMEngineAPI::IConfigManager> configMgr = configManager;
    if (!configMgr) {
        // Wrap singleton in shared_ptr with no-op deleter for compatibility
        configMgr = std::shared_ptr<::LLMEngineAPI::IConfigManager>(
            &::LLMEngineAPI::APIConfigManager::getInstance(),
            [](::LLMEngineAPI::IConfigManager*) {});
    }

    // Load config
    if (!configMgr->loadConfig()) {
        if (logger) {
            logger->log(LogLevel::Warn, "Could not load API config, using defaults");
        }
    }

    // Determine provider name (use default if empty)
    std::string resolvedProvider(providerName);
    if (resolvedProvider.empty()) {
        resolvedProvider = configMgr->getDefaultProvider();
        if (resolvedProvider.empty()) {
            resolvedProvider = "ollama";
        }
    }

    // Get provider configuration
    auto providerConfig = configMgr->getProviderConfig(resolvedProvider);
    if (providerConfig.empty()) {
        std::string errorMsg =
            std::string("Provider ") + resolvedProvider + " not found in config";
        if (logger) {
            logger->log(LogLevel::Error, errorMsg);
        }
        throw std::runtime_error("Invalid provider name");
    }

    // Set provider type
    result.providerType =
        ::LLMEngineAPI::APIClientFactory::stringToProviderType(resolvedProvider);

    // Resolve API key with priority: env var → param → config
    std::string apiKeyFromConfig =
        providerConfig.value(std::string(Constants::JsonKeys::apiKey), "");
    result.apiKey = resolveApiKey(result.providerType, apiKey, apiKeyFromConfig, logger);

    // Set model
    std::string modelFromConfig =
        providerConfig.value(std::string(Constants::JsonKeys::defaultModel), "");
    result.model = resolveModel(result.providerType, model, modelFromConfig, logger);

    // Set Ollama URL if using Ollama (or generic Base URL)
    // For now, only Ollama and OpenAI-likes really use BaseURL configuration dynamically
    std::string baseUrlFromConfig =
        providerConfig.value(std::string(Constants::JsonKeys::baseUrl), "");

    // Always resolve base URL (e.g. for OpenAI compatible endpoints or Ollama)
    result.ollamaUrl = resolveBaseUrl(result.providerType, "", baseUrlFromConfig, logger);

    return result;
}

SecureString ProviderBootstrap::resolveApiKey(::LLMEngineAPI::ProviderType providerType,
                                              std::string_view apiKeyFromParam,
                                              std::string_view apiKeyFromConfig,
                                              Logger* logger) {

    std::string envVarName = getApiKeyEnvVarName(providerType);

    // SECURITY: Prefer environment variables for API keys over config file or constructor parameter
    // Only use provided apiKey if environment variable is not set
    const char* envApiKey = nullptr;
    if (!envVarName.empty()) {
        envApiKey = std::getenv(envVarName.c_str());
    }

    if (envApiKey && strlen(envApiKey) > 0) {
        return SecureString(envApiKey);
    }
    if (!std::string(apiKeyFromParam).empty()) {
        // Use provided API key if environment variable is not set
        return SecureString(apiKeyFromParam);
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

std::string ProviderBootstrap::resolveBaseUrl(::LLMEngineAPI::ProviderType providerType,
                                              std::string_view baseUrlFromParam,
                                              std::string_view baseUrlFromConfig,
                                              Logger* /*logger*/) {
    std::string envVarName = getBaseUrlEnvVarName(providerType);
    const char* envVal = nullptr;
    if (!envVarName.empty()) {
        envVal = std::getenv(envVarName.c_str());
    }

    // 1. Env Var
    if (envVal && strlen(envVal) > 0) {
        return std::string(envVal);
    }
    // 2. Param
    if (!baseUrlFromParam.empty()) {
        return std::string(baseUrlFromParam);
    }
    // 3. Config
    if (!baseUrlFromConfig.empty()) {
        return std::string(baseUrlFromConfig);
    }

    // 4. Default
    switch (providerType) {
        case ::LLMEngineAPI::ProviderType::ollama:
            return std::string(Constants::DefaultUrls::ollamaBase);
        case ::LLMEngineAPI::ProviderType::qwen:
            return std::string(Constants::DefaultUrls::qwenBase);
        case ::LLMEngineAPI::ProviderType::openai:
            return std::string(Constants::DefaultUrls::openaiBase);
        case ::LLMEngineAPI::ProviderType::anthropic:
            return std::string(Constants::DefaultUrls::anthropicBase);
        case ::LLMEngineAPI::ProviderType::gemini:
            return std::string(Constants::DefaultUrls::geminiBase);
        default:
            return "";
    }
}

std::string ProviderBootstrap::resolveModel(::LLMEngineAPI::ProviderType providerType,
                                            std::string_view modelFromParam,
                                            std::string_view modelFromConfig,
                                            Logger* /*logger*/) {
    std::string envVarName = getModelEnvVarName(providerType);
    const char* envVal = nullptr;
    if (!envVarName.empty()) {
        envVal = std::getenv(envVarName.c_str());
    }

    // 1. Provider-specific Env Var
    if (envVal && strlen(envVal) > 0) {
        return std::string(envVal);
    }

    // 1b. Global Default Env Var (defaultModel)
    const std::string envVar = std::string(Constants::EnvVars::defaultModel);
    const char* globalEnv = std::getenv(envVar.c_str());
    if (globalEnv && strlen(globalEnv) > 0) {
        return std::string(globalEnv);
    }

    // 2. Param
    if (!modelFromParam.empty()) {
        return std::string(modelFromParam);
    }
    // 3. Config
    if (!modelFromConfig.empty()) {
        return std::string(modelFromConfig);
    }

    // 4. Default
    switch (providerType) {
        case ::LLMEngineAPI::ProviderType::qwen:
            return std::string(Constants::DefaultModels::qwen);
        case ::LLMEngineAPI::ProviderType::openai:
            return std::string(Constants::DefaultModels::openai);
        case ::LLMEngineAPI::ProviderType::anthropic:
            return std::string(Constants::DefaultModels::anthropic);
        case ::LLMEngineAPI::ProviderType::gemini:
            return std::string(Constants::DefaultModels::gemini);
        case ::LLMEngineAPI::ProviderType::ollama:
            return std::string(Constants::DefaultModels::ollama);
        default:
            return "";
    }
}

std::string ProviderBootstrap::getApiKeyEnvVarName(::LLMEngineAPI::ProviderType providerType) {
    switch (providerType) {
        case ::LLMEngineAPI::ProviderType::qwen:
            return std::string(Constants::EnvVars::qwenApiKey);
        case ::LLMEngineAPI::ProviderType::openai:
            return std::string(Constants::EnvVars::openaiApiKey);
        case ::LLMEngineAPI::ProviderType::anthropic:
            return std::string(Constants::EnvVars::anthropicApiKey);
        case ::LLMEngineAPI::ProviderType::gemini:
            return std::string(Constants::EnvVars::geminiApiKey);
        default:
            return ""; // Ollama doesn't use API keys
    }
}

std::string ProviderBootstrap::getBaseUrlEnvVarName(::LLMEngineAPI::ProviderType providerType) {
    switch (providerType) {
        case ::LLMEngineAPI::ProviderType::ollama:
            return std::string(Constants::EnvVars::ollamaHost);
        case ::LLMEngineAPI::ProviderType::openai:
            return std::string(Constants::EnvVars::openaiBaseUrl);
        case ::LLMEngineAPI::ProviderType::qwen:
            return std::string(Constants::EnvVars::qwenBaseUrl);
        case ::LLMEngineAPI::ProviderType::anthropic:
            return std::string(Constants::EnvVars::anthropicBaseUrl);
        case ::LLMEngineAPI::ProviderType::gemini:
            return std::string(Constants::EnvVars::geminiBaseUrl);
        default:
            return "";
    }
}

std::string ProviderBootstrap::getModelEnvVarName(::LLMEngineAPI::ProviderType providerType) {
    switch (providerType) {
        case ::LLMEngineAPI::ProviderType::ollama:
            return std::string(Constants::EnvVars::ollamaModel);
        case ::LLMEngineAPI::ProviderType::openai:
            return std::string(Constants::EnvVars::openaiModel);
        case ::LLMEngineAPI::ProviderType::qwen:
            return std::string(Constants::EnvVars::qwenModel);
        case ::LLMEngineAPI::ProviderType::anthropic:
            return std::string(Constants::EnvVars::anthropicModel);
        case ::LLMEngineAPI::ProviderType::gemini:
            return std::string(Constants::EnvVars::geminiModel);
        default:
            return "";
    }
}

} // namespace LLMEngine
