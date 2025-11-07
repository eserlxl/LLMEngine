#include "../src/APIClient.hpp"

#include <cassert>
#include <iostream>

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

    // Fallback to default provider when unknown
    APIConfigManager& mgr = APIConfigManager::getInstance();
    (void)mgr.loadConfig("/opt/lxl/c++/LLMEngine/config/api_config.json");
    auto ty = APIClientFactory::stringToProviderType("not-a-provider");
    (void)ty; // cannot assert exact fallback without knowing default; only ensure it returns a
              // valid enum value by converting back
    auto s = APIClientFactory::providerTypeToString(ty);
    assert(!s.empty());

    std::cout << "OK\n";
    return 0;
}
