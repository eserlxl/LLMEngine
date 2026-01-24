// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LLMEngine/LLMEngine.hpp"
#include "support/FakeAPIClient.hpp"
#include <cassert>
#include <iostream>

using namespace LLMEngine;
using namespace LLMEngineAPI;

void testTimeoutPropagation() {
    auto clientPtr = std::make_unique<FakeAPIClient>();
    FakeAPIClient* fakeClient = clientPtr.get();
    LLMEngine::LLMEngine engine(std::move(clientPtr));

    RequestOptions opts;
    opts.timeout_ms = 5000;
    
    // Test sync
    engine.analyze("prompt", {}, "test", opts);
    assert(fakeClient->getLastOptions().timeout_ms.has_value());
    assert(*fakeClient->getLastOptions().timeout_ms == 5000);

    // Test async
    opts.timeout_ms = 10000;
    auto f = engine.analyzeAsync("prompt", {}, "test", opts);
    f.get();
    assert(fakeClient->getLastOptions().timeout_ms.has_value());
    assert(*fakeClient->getLastOptions().timeout_ms == 10000);
    
    // Test default (verifies the default argument = {} works)
    engine.analyze("prompt", {}, "test");
    assert(!fakeClient->getLastOptions().timeout_ms.has_value());
    
    std::cout << "testTimeoutPropagation passed\n";
}

int main() {
    try {
        testTimeoutPropagation();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
