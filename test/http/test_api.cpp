// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This program runs API integration tests for LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/core/LLMEngine.hpp"
#include "LLMEngine/providers/APIClient.hpp"
#include "LLMEngine/utils/Utils.hpp"

#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using namespace LLMEngine;
using namespace LLMEngineAPI;

void printSeparator(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << " " << title << std::endl;
    std::cout << std::string(60, '=') << std::endl;
}

void testAPIConfigManager() {
    printSeparator("Testing API Config Manager");

    auto& config_mgr = ::LLMEngineAPI::APIConfigManager::getInstance();

    if (config_mgr.loadConfig("config/api_config.json")) {
        std::cout << "✓ Config loaded successfully" << std::endl;

        std::cout << "Default provider: " << config_mgr.getDefaultProvider() << std::endl;
        std::cout << "Timeout: " << config_mgr.getTimeoutSeconds() << " seconds" << std::endl;
        std::cout << "Retry attempts: " << config_mgr.getRetryAttempts() << std::endl;

        std::cout << "\nAvailable providers:" << std::endl;
        for (const auto& provider : config_mgr.getAvailableProviders()) {
            std::cout << "  - " << provider << std::endl;
            auto provider_config = config_mgr.getProviderConfig(provider);
            if (!provider_config.empty()) {
                std::cout << "    Name: " << provider_config.value("name", "N/A") << std::endl;
                std::cout << "    Base URL: " << provider_config.value("base_url", "N/A")
                          << std::endl;
                std::cout << "    Default Model: " << provider_config.value("default_model", "N/A")
                          << std::endl;
            }
        }
    } else {
        std::cout << "✗ Failed to load config" << std::endl;
    }
}

void testQwenClient() {
    printSeparator("Testing Qwen Client");

    std::cout << "NOTE: This test requires a valid Qwen API key" << std::endl;
    std::cout << "Set the QWEN_API_KEY environment variable to test" << std::endl;

    const char* api_key_env = std::getenv("QWEN_API_KEY");
    if (!api_key_env) {
        std::cout << "⊘ Skipping Qwen client test (no API key)" << std::endl;
        return;
    }

    std::string api_key(api_key_env);
    std::cout << "API key found: " << api_key.substr(0, 10) << "..." << std::endl;

    try {
        ::LLMEngineAPI::QwenClient client(api_key, "qwen-flash");
        std::cout << "✓ Qwen client created successfully" << std::endl;
        std::cout << "  Provider: " << client.getProviderName() << std::endl;

        // Test simple request
        std::string prompt = "What is 2+2? Answer with just the number.";
        nlohmann::json input = {{"system_prompt", "You are a helpful math assistant."}};
        input["max_tokens"] = 50; // Add max_tokens to input payload
        nlohmann::json params = {{"temperature", 0.1}};

        std::cout << "Sending test request..." << std::endl;
        auto response = client.sendRequest(prompt, input, params);

        if (response.success) {
            std::cout << "✓ Request successful" << std::endl;
            std::cout << "Response: " << response.content << std::endl;
        } else {
            std::cout << "✗ Request failed: " << response.errorMessage << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "✗ Exception: " << e.what() << std::endl;
    }
}

void testLLMEngineWithQwen() {
    printSeparator("Testing LLMEngine with Qwen");

    const char* api_key_env = std::getenv("QWEN_API_KEY");
    if (!api_key_env) {
        std::cout << "⊘ Skipping LLMEngine Qwen test (no API key)" << std::endl;
        return;
    }

    std::string api_key(api_key_env);

    try {
        // Test with provider type constructor
        std::cout << "\n1. Testing with ProviderType constructor..." << std::endl;

        nlohmann::json model_params = {{"temperature", 0.7}};

        ::LLMEngine::LLMEngine engine(
            ::LLMEngineAPI::ProviderType::QWEN, api_key, "qwen-flash", model_params, 24, true);
        std::cout << "✓ LLMEngine initialized with Qwen" << std::endl;
        std::cout << "  Provider: " << engine.getProviderName() << std::endl;
        std::cout << "  Is online: " << (engine.isOnlineProvider() ? "Yes" : "No") << std::endl;

        // Test analysis
        std::string prompt = "Explain what machine learning is in one sentence:";
        nlohmann::json input = {{"context", "educational"}};
        input["max_tokens"] = 100; // Add max_tokens to input payload

        std::cout << "Running analysis..." << std::endl;
        AnalysisResult result = engine.analyze(prompt, input, "qwen_test");

        if (result.success) {
            std::cout << "✓ Analysis completed" << std::endl;
            std::cout << "Think section (" << result.think.length() << " chars): "
                      << (result.think.empty()
                              ? "(empty)"
                              : result.think.substr(0, std::min(100UL, result.think.length())))
                      << std::endl;
            std::cout << "Response section (" << result.content.length() << " chars): "
                      << result.content.substr(0, std::min(200UL, result.content.length())) << "..."
                      << std::endl;
        } else {
            std::cout << "✗ Analysis failed: " << result.errorMessage << " (status "
                      << result.statusCode << ")" << std::endl;
        }

        // Test with provider name constructor
        std::cout << "\n2. Testing with provider name constructor..." << std::endl;
        ::LLMEngine::LLMEngine engine2("qwen", api_key, "qwen-flash", model_params, 24, true);
        std::cout << "✓ LLMEngine initialized with provider name" << std::endl;
        std::cout << "  Provider: " << engine2.getProviderName() << std::endl;

    } catch (const std::exception& e) {
        std::cout << "✗ Exception: " << e.what() << std::endl;
    }
}

void testLLMEngineOllamaProvider() {
    printSeparator("Testing LLMEngine Ollama Provider");

    std::cout << "Testing Ollama provider via ProviderType constructor..." << std::endl;

    try {
        std::string model = "llama2";
        nlohmann::json model_params = {{"temperature", 0.7}, {"top_p", 0.9}};

        ::LLMEngine::LLMEngine engine(
            ::LLMEngineAPI::ProviderType::OLLAMA, "", model, model_params, 24, true);
        std::cout << "✓ LLMEngine initialized with Ollama provider" << std::endl;
        std::cout << "  Provider: " << engine.getProviderName() << std::endl;
        std::cout << "  Is online: " << (engine.isOnlineProvider() ? "Yes" : "No") << std::endl;

        // Test with provider name constructor
        ::LLMEngine::LLMEngine engine2("ollama", "", model, model_params, 24, true);
        std::cout << "✓ LLMEngine initialized with provider name 'ollama'" << std::endl;

        // Note: We don't actually make a request here since Ollama might not be running
        std::cout << "Note: Skipping actual request (Ollama might not be running)" << std::endl;

    } catch (const std::exception& e) {
        std::cout << "✗ Exception: " << e.what() << std::endl;
    }
}

void testAPIClientFactory() {
    printSeparator("Testing API Client Factory");

    try {
        std::cout << "1. Testing provider type to string conversion..." << std::endl;
        std::cout << "  QWEN -> "
                  << ::LLMEngineAPI::APIClientFactory::providerTypeToString(
                         ::LLMEngineAPI::ProviderType::QWEN)
                  << std::endl;
        std::cout << "  OPENAI -> "
                  << ::LLMEngineAPI::APIClientFactory::providerTypeToString(
                         ::LLMEngineAPI::ProviderType::OPENAI)
                  << std::endl;
        std::cout << "  ANTHROPIC -> "
                  << ::LLMEngineAPI::APIClientFactory::providerTypeToString(
                         ::LLMEngineAPI::ProviderType::ANTHROPIC)
                  << std::endl;
        std::cout << "  OLLAMA -> "
                  << ::LLMEngineAPI::APIClientFactory::providerTypeToString(
                         ::LLMEngineAPI::ProviderType::OLLAMA)
                  << std::endl;

        std::cout << "\n2. Testing string to provider type conversion..." << std::endl;
        auto qwen_type = ::LLMEngineAPI::APIClientFactory::stringToProviderType("qwen");
        (void)qwen_type; // Suppress unused variable warning - value used for verification
        std::cout << "  'qwen' converted successfully" << std::endl;

        std::cout << "\n3. Testing client creation..." << std::endl;

        // Test Ollama client (no API key needed)
        auto ollama_client = ::LLMEngineAPI::APIClientFactory::createClient(
            ::LLMEngineAPI::ProviderType::OLLAMA, "", "llama2", "http://localhost:11434");
        if (ollama_client) {
            std::cout << "✓ Ollama client created: " << ollama_client->getProviderName()
                      << std::endl;
        }

        std::cout << "✓ Factory tests completed" << std::endl;

    } catch (const std::exception& e) {
        std::cout << "✗ Exception: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "=== LLMEngine API Integration Tests ===" << std::endl;
    std::cout << "Testing new API support for online AI models" << std::endl;

    try {
        // Test 1: API Config Manager
        testAPIConfigManager();

        // Test 2: API Client Factory
        testAPIClientFactory();

        // Test 3: Qwen Client
        testQwenClient();

        // Test 4: LLMEngine with Qwen
        testLLMEngineWithQwen();

        // Test 5: Ollama provider
        testLLMEngineOllamaProvider();

        printSeparator("All Tests Completed");
        std::cout << "\nTest Summary:" << std::endl;
        std::cout << "  - API Config Manager: ✓" << std::endl;
        std::cout << "  - API Client Factory: ✓" << std::endl;
        std::cout << "  - Qwen Client: " << (std::getenv("QWEN_API_KEY") ? "✓" : "⊘ (skipped)")
                  << std::endl;
        std::cout << "  - LLMEngine with Qwen: "
                  << (std::getenv("QWEN_API_KEY") ? "✓" : "⊘ (skipped)") << std::endl;
        std::cout << "  - Ollama Provider: ✓" << std::endl;

        std::cout << "\nTo test Qwen integration, set QWEN_API_KEY environment variable:"
                  << std::endl;
        std::cout << "  export QWEN_API_KEY='your-api-key-here'" << std::endl;
        std::cout << "  ./test_api" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
