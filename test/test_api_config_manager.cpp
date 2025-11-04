// Copyright © 2025 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of the LLMEngine test suite (config manager tests) and is
// licensed under the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/APIClient.hpp"
#include <cassert>
#include <iostream>

using namespace LLMEngineAPI;

int main() {
    // Error path: non-existent file
    {
        auto& mgr = APIConfigManager::getInstance();
        bool ok = mgr.loadConfig("/tmp/this-file-should-not-exist-llmengine.json");
        assert(ok == false);
        // Defaults when not loaded
        assert(mgr.getDefaultProvider() == std::string("ollama"));
        assert(mgr.getTimeoutSeconds() == 30);
        assert(mgr.getRetryAttempts() == 3);
        assert(mgr.getRetryDelayMs() == 1000);
        auto cfg = mgr.getProviderConfig("qwen");
        assert(cfg.is_null() || cfg.empty());
    }

    // Test default config path API
    {
        auto& mgr = APIConfigManager::getInstance();
        
        // Check default path is set to expected value
        std::string default_path = mgr.getDefaultConfigPath();
        assert(default_path == "config/api_config.json");
        
        // Change the default path
        mgr.setDefaultConfigPath("/custom/path/config.json");
        assert(mgr.getDefaultConfigPath() == "/custom/path/config.json");
        
        // Reset to original default
        mgr.setDefaultConfigPath("config/api_config.json");
        assert(mgr.getDefaultConfigPath() == "config/api_config.json");
    }

    // Test provider-specific timeout
    {
        auto& mgr = APIConfigManager::getInstance();
        bool ok = mgr.loadConfig("config/api_config.json");
        if (ok) {
            // Global timeout should be 30 seconds
            assert(mgr.getTimeoutSeconds() == 30);
            
            // Ollama should have provider-specific timeout of 300 seconds
            int ollama_timeout = mgr.getTimeoutSeconds("ollama");
            assert(ollama_timeout == 300);
            
            // Other providers should fall back to global timeout (30 seconds)
            int qwen_timeout = mgr.getTimeoutSeconds("qwen");
            assert(qwen_timeout == 30);
            
            // Non-existent provider should fall back to global timeout
            int unknown_timeout = mgr.getTimeoutSeconds("nonexistent");
            assert(unknown_timeout == 30);
        }
    }

    std::cout << "test_api_config_manager: OK\n";
    return 0;
}


