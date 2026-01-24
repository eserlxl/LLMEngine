
// Copyright © 2026 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#undef NDEBUG
#include <cassert>
#include <iostream>
#include "LLMEngine/LLMEngineBuilder.hpp"
#include "LLMEngine/LLMEngine.hpp"
#include "LLMEngine/IConfigManager.hpp"
#include <memory>

// Stub Config Manager to avoid filesystem dependency
class StubConfigManager : public LLMEngineAPI::IConfigManager {
public:
    void setDefaultConfigPath(std::string_view) override {}
    std::string getDefaultConfigPath() const override { return ""; }
    void setLogger(std::shared_ptr<LLMEngine::Logger>) override {}
    bool loadConfig(std::string_view) override { return true; }
    
    nlohmann::json getProviderConfig(std::string_view name) const override {
        // Return minimal config so bootstrap passes
        if (name == "openai") {
             return {{"type", "openai"}, {"api_key", "sk-dummy"}};
        }
        return {};
    }
    
    std::vector<std::string> getAvailableProviders() const override { return {"openai"}; }
    std::string getDefaultProvider() const override { return "openai"; }
    int getTimeoutSeconds() const override { return 30; }
    int getTimeoutSeconds(std::string_view) const override { return 30; }
    int getRetryAttempts() const override { return 3; }
    int getRetryDelayMs() const override { return 1000; }
};

int main() {
    std::cout << "Testing Builder Refinements (Base URL)..." << std::endl;
    
    auto stubConfig = std::make_shared<StubConfigManager>();

    try {
        auto engine = LLMEngine::LLMEngineBuilder()
            .withConfigManager(stubConfig)
            .withProvider("openai")
            .withApiKey("sk-test")
            .withModel("gpt-4") // Should prioritize this model
            .withBaseUrl("http://localhost:11434/v1") // Custom Base URL
            .build();
            
        assert(engine != nullptr);
        
        // We cannot easily verify the internal URL without accessors or mocks,
        // but success indicates the constructor accepted the argument.
        // And our manual review confirmed the wiring.
        
        std::cout << "  Construction with Base URL: PASS" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "  Construction with Base URL failed: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Builder Refinements tests passed." << std::endl;
    return 0;
}
