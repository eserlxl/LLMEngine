#include "../src/APIClient.hpp"
#include <cassert>
#include <iostream>

int main() {
    using namespace LLMEngineAPI;
    APIConfigManager& mgr = APIConfigManager::getInstance();

    const std::string config_path = "/opt/lxl/c++/LLMEngine/config/api_config.json";
    bool ok = mgr.loadConfig(config_path);
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
    assert(tout > 0);
    assert(retries >= 0);
    assert(delay >= 0);

    std::cout << "OK\n";
    return 0;
}


