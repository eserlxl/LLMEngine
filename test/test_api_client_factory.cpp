// Copyright © 2025 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of the LLMEngine test suite (factory mapping tests) and is
// licensed under the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/APIClient.hpp"
#include <cassert>
#include <iostream>

using namespace LLMEngineAPI;

int main() {
    // stringToProviderType
    assert(APIClientFactory::stringToProviderType("qwen") == ProviderType::QWEN);
    assert(APIClientFactory::stringToProviderType("openai") == ProviderType::OPENAI);
    assert(APIClientFactory::stringToProviderType("anthropic") == ProviderType::ANTHROPIC);
    assert(APIClientFactory::stringToProviderType("ollama") == ProviderType::OLLAMA);

    // providerTypeToString
    assert(APIClientFactory::providerTypeToString(ProviderType::QWEN) == std::string("qwen"));
    assert(APIClientFactory::providerTypeToString(ProviderType::OPENAI) == std::string("openai"));
    assert(APIClientFactory::providerTypeToString(ProviderType::ANTHROPIC) ==
           std::string("anthropic"));
    assert(APIClientFactory::providerTypeToString(ProviderType::OLLAMA) == std::string("ollama"));

    // Test that createClientFromConfig throws when no credentials are provided for non-Ollama
    // providers This is the new security behavior - fail fast instead of proceeding silently
    {
        nlohmann::json empty_config = nlohmann::json::object();
        bool exception_thrown = false;
        try {
            auto client = APIClientFactory::createClientFromConfig("qwen", empty_config);
            (void)client; // Should not reach here
        } catch (const std::runtime_error& e) {
            exception_thrown = true;
            std::string error_msg = e.what();
            assert(error_msg.find("No API key found") != std::string::npos);
        }
        assert(exception_thrown &&
               "createClientFromConfig should throw when no credentials provided");
    }

    // Test that Ollama still works without credentials (it's local)
    {
        nlohmann::json ollama_config = {{"base_url", "http://localhost:11434"},
                                        {"default_model", "llama2"}};
        auto ollama_client = APIClientFactory::createClientFromConfig("ollama", ollama_config);
        assert(ollama_client != nullptr);
        assert(ollama_client->getProviderType() == ProviderType::OLLAMA);
    }

    std::cout << "test_api_client_factory: OK\n";
    return 0;
}
