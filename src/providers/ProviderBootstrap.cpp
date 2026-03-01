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

    // 1b. Global Default Env Var (DEFAULT_MODEL)
    const std::string envVar = std::string(Constants::EnvVars::DEFAULT_MODEL);
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

std::string ProviderBootstrap::getApiKeyEnvVarName(::LLMEngineAPI::ProviderType providerType) {
    switch (providerType) {
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

std::string ProviderBootstrap::getBaseUrlEnvVarName(::LLMEngineAPI::ProviderType providerType) {
    switch (providerType) {
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

std::string ProviderBootstrap::getModelEnvVarName(::LLMEngineAPI::ProviderType providerType) {
    switch (providerType) {
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
