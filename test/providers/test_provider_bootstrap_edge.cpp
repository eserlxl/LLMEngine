// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of the LLMEngine test suite (Provider Bootstrap tests) and is
// licensed under the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "llmengine/providers/provider_bootstrap.hpp"
#include "llmengine/providers/api_client.hpp"
#include "llmengine/utils/logger.hpp"

#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <cstdlib>

// Mock logger that captures output
class TestLogger : public LLMEngine::Logger {
  public:
    void log(LLMEngine::LogLevel level, std::string_view message) override {
        lastMessage = std::string(message);
        lastLevel = level;
    }

    std::string lastMessage;
    LLMEngine::LogLevel lastLevel;
};

void testResolveApiKeyEdgeCases() {
    TestLogger logger;
    
    // Both param and config are empty. Should return empty string for OpenAI because no env var is set.
    // Unset environment variable first just in case
    unsetenv("openaiApiKey");
    
    auto apiKey = LLMEngine::ProviderBootstrap::resolveApiKey(
        LLMEngineAPI::ProviderType::openai,
        "",
        "",
        &logger
    );
    assert(apiKey.view().empty());
    
    // Now with param empty, but config has value
    apiKey = LLMEngine::ProviderBootstrap::resolveApiKey(
        LLMEngineAPI::ProviderType::openai,
        "",
        "config_key",
        &logger
    );
    assert(apiKey.view() == "config_key");
    assert(logger.lastLevel == LLMEngine::LogLevel::Warn);
    assert(logger.lastMessage.find("security risk") != std::string::npos);
    
    std::cout << "✓ API Key resolution edge cases passed\n";
}

void testResolveBaseUrlEdgeCases() {
    TestLogger logger;
    
    // Unset environment variables
    unsetenv("openaiBaseUrl");
    
    // All empty -> Fallback to default
    std::string baseUrl = LLMEngine::ProviderBootstrap::resolveBaseUrl(
        LLMEngineAPI::ProviderType::openai,
        "",
        "",
        &logger
    );
    assert(baseUrl == "https://api.openai.com/v1"); // Constants::DefaultUrls::openaiBase
    
    std::cout << "✓ Base URL resolution edge cases passed\n";
}

int main() {
    std::cout << "=== Provider Bootstrap Edge Cases ===\n";
    testResolveApiKeyEdgeCases();
    testResolveBaseUrlEdgeCases();
    std::cout << "\nAll provider bootstrap edge cases passed!\n";
    return 0;
}
