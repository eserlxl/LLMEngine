#include "LLMEngine/APIClient.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>

int main() {
    using namespace LLMEngineAPI;

    // Exact matches
    assert(APIClientFactory::stringToProviderType("qwen") == ProviderType::QWEN);
    assert(APIClientFactory::stringToProviderType("openai") == ProviderType::OPENAI);
    assert(APIClientFactory::stringToProviderType("anthropic") == ProviderType::ANTHROPIC);
    assert(APIClientFactory::stringToProviderType("ollama") == ProviderType::OLLAMA);
    assert(APIClientFactory::stringToProviderType("gemini") == ProviderType::GEMINI);

    // Case-insensitive
    assert(APIClientFactory::stringToProviderType("QWEN") == ProviderType::QWEN);
    assert(APIClientFactory::stringToProviderType("OpenAI") == ProviderType::OPENAI);
    assert(APIClientFactory::stringToProviderType("GEMINI") == ProviderType::GEMINI);

    // Test that unknown providers throw an exception (current behavior)
    bool exception_thrown = false;
    try {
        (void)APIClientFactory::stringToProviderType("not-a-provider");
    } catch (const std::runtime_error& e) {
        exception_thrown = true;
        std::string error_msg = e.what();
        assert(error_msg.find("Unknown provider") != std::string::npos);
        assert(error_msg.find("not-a-provider") != std::string::npos);
    }
    assert(exception_thrown);

    // Test that empty string falls back to default provider
    APIConfigManager& mgr = APIConfigManager::getInstance();
    // Try to load config from possible paths (works in both local and CI environments)
    // If it fails, that's OK - we'll use the default default provider ("ollama")
    bool config_loaded = false;
    // Try relative path from source directory
    if (!config_loaded) {
        config_loaded = mgr.loadConfig("config/api_config.json");
    }
    // Try relative path from build directory (CI runs from build/)
    if (!config_loaded) {
        config_loaded = mgr.loadConfig("../config/api_config.json");
    }
    
    // Empty string should fall back to default provider
    ProviderType default_ty = APIClientFactory::stringToProviderType("");
    // Should return a valid provider type
    std::string default_s = APIClientFactory::providerTypeToString(default_ty);
    assert(!default_s.empty());
    // Should be one of the known providers
    assert(default_s == "qwen" || default_s == "openai" || default_s == "anthropic" 
           || default_s == "ollama" || default_s == "gemini");

    std::cout << "OK\n";
    return 0;
}
