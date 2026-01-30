
// Copyright © 2026 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#undef NDEBUG
#include <cassert>
#include <iostream>
#include "LLMEngine/core/LLMEngineBuilder.hpp"
#include "LLMEngine/core/LLMEngine.hpp"
#include "LLMEngine/core/IConfigManager.hpp"

class StubConfigManager : public LLMEngineAPI::IConfigManager {
public:
    void setDefaultConfigPath(std::string_view) override {}
    std::string getDefaultConfigPath() const override { return ""; }
    void setLogger(std::shared_ptr<LLMEngine::Logger>) override {}
    bool loadConfig(std::string_view) override { return true; }
    
    nlohmann::json getProviderConfig(std::string_view name) const override {
        if (name == "openai") {
            return {
                {"type", "openai"},
                {"api_key", "sk-dummy"}, // Default if not overridden
                {"model", "gpt-3.5-turbo"}
            };
        }
        if (name == "qwen") {
             return {
                {"type", "qwen"}
             };
        }
        return {};
    }
    
    std::vector<std::string> getAvailableProviders() const override { return {"openai", "qwen"}; }
    std::string getDefaultProvider() const override { return "openai"; }
    int getTimeoutSeconds() const override { return 30; }
    int getTimeoutSeconds(std::string_view) const override { return 30; }
    int getRetryAttempts() const override { return 3; }
    int getRetryDelayMs() const override { return 1000; }
};

int main() {
    std::cout << "Testing LLMEngineBuilder..." << std::endl;
    
    auto stubConfig = std::make_shared<StubConfigManager>();

    try {
        auto engine = LLMEngine::LLMEngineBuilder()
            .withConfigManager(stubConfig)
            .withProvider("openai")
            .withApiKey("sk-test")
            .withModel("gpt-4")
            .build();
            
        assert(engine != nullptr);
        assert(engine->getProviderType() == LLMEngineAPI::ProviderType::OPENAI);
        assert(engine->getModelName() == "gpt-4");
        
        std::cout << "  Basic build: PASS" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "  Basic build failed: " << e.what() << std::endl;
        return 1;
    }

    try {
        auto engine = LLMEngine::LLMEngineBuilder()
            .withConfigManager(stubConfig)
            .withProvider("qwen")
            .withApiKey("sk-qwen-test") 
            .build();
        assert(engine->getProviderType() == LLMEngineAPI::ProviderType::QWEN);
        std::cout << "  Qwen build: PASS" << std::endl;
    } catch (const std::exception& e) {
         std::cerr << "  Qwen build failed: " << e.what() << std::endl;
         return 1;
    }

    // Test missing provider exception (still throws because builder checks name)
    try {
        (void)LLMEngine::LLMEngineBuilder()
            .withConfigManager(stubConfig)
            .build();
        std::cerr << "  Missing provider check: FAIL (Should have thrown)" << std::endl;
        return 1;
    } catch (const std::exception&) {
        std::cout << "  Missing provider check: PASS" << std::endl;
    }

    std::cout << "LLMEngineBuilder tests passed." << std::endl;
    return 0;
}
