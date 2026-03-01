// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
#include "llmengine/providers/api_config_manager.hpp"
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

