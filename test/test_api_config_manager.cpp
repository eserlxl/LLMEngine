// Copyright © 2025 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of the LLMEngine test suite (config manager tests) and is
// licensed under the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "APIClient.hpp"
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

    std::cout << "test_api_config_manager: OK\n";
    return 0;
}


