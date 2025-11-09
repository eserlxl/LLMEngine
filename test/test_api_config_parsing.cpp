#include "LLMEngine/APIClient.hpp"

#include <cassert>
#include <iostream>
#include <vector>

int main() {
    using namespace LLMEngineAPI;
    APIConfigManager& mgr = APIConfigManager::getInstance();

    // Try to load config from possible paths (works in both local and CI environments)
    bool ok = false;
    // Try multiple path combinations to handle different test execution contexts
    // 1. Relative path from source directory (when run from project root)
    if (!ok) {
        ok = mgr.loadConfig("config/api_config.json");
    }
    // 2. Relative path from build directory (when ctest runs from build/)
    if (!ok) {
        ok = mgr.loadConfig("../config/api_config.json");
    }
    // 3. Relative path when executable is in build/test/ subdirectory
    if (!ok) {
        ok = mgr.loadConfig("../../config/api_config.json");
    }
    // 4. Try absolute path by finding project root (look for config directory)
    if (!ok) {
        // Try to find the config directory by going up from current directory
        // This handles cases where the test is run from various locations
        std::vector<std::string> paths_to_try = {
            "../../../config/api_config.json",  // build/test/Debug or similar
            "../../../../config/api_config.json", // build/test/bin/Debug or similar
        };
        for (const auto& path : paths_to_try) {
            if (!ok) {
                ok = mgr.loadConfig(path);
            }
        }
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
