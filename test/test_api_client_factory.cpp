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
    assert(APIClientFactory::providerTypeToString(ProviderType::ANTHROPIC) == std::string("anthropic"));
    assert(APIClientFactory::providerTypeToString(ProviderType::OLLAMA) == std::string("ollama"));

    std::cout << "test_api_client_factory: OK\n";
    return 0;
}


