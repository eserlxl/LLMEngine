#include "LLMEngine/APIClient.hpp"

#include <cassert>
#include <iostream>

int main() {
    using namespace LLMEngineAPI;
    APIConfigManager& mgr = APIConfigManager::getInstance();

    // Try to load config from possible paths (works in both local and CI environments)
    bool ok = false;
    // Try relative path from source directory
    if (!ok) {
        ok = mgr.loadConfig("config/api_config.json");
    }
    // Try relative path from build directory (CI runs from build/)
    if (!ok) {
        ok = mgr.loadConfig("../config/api_config.json");
    }
    (void)ok; // Suppress unused variable warning - value checked in assert
    assert(ok && "Config should load successfully");

    // Default provider must not be empty
    std::string def = mgr.getDefaultProvider();
    assert(!def.empty());

    // Get provider config for default
    auto cfg = mgr.getProviderConfig(def);
    assert(!cfg.is_null());

    // Global settings should have sane defaults
    int tout = mgr.getTimeoutSeconds();
    int retries = mgr.getRetryAttempts();
    int delay = mgr.getRetryDelayMs();
    (void)tout; // Suppress unused variable warning - value checked in assert
    (void)retries; // Suppress unused variable warning - value checked in assert
    (void)delay; // Suppress unused variable warning - value checked in assert
    assert(tout > 0);
    assert(retries >= 0);
    assert(delay >= 0);

    std::cout << "OK\n";
    return 0;
}
