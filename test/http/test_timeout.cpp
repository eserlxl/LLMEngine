// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LLMEngine/core/LLMEngine.hpp"
#include "support/FakeAPIClient.hpp"
#include <gtest/gtest.h>

using namespace LLMEngine;
using namespace LLMEngineAPI;

class TimeoutTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup shared resources if needed
    }
};

TEST_F(TimeoutTest, Propagation) {
    auto clientPtr = std::make_unique<FakeAPIClient>();
    auto* fakeClient = clientPtr.get();
    LLMEngine::LLMEngine engine(std::move(clientPtr));

    RequestOptions opts;
    opts.timeout_ms = 5000;
    
    // Test sync
    auto result = engine.analyze("prompt", {}, "test", opts);
    (void)result;
    
    ASSERT_TRUE(fakeClient->getLastOptions().timeout_ms.has_value());
    ASSERT_EQ(*fakeClient->getLastOptions().timeout_ms, 5000);

    // Test async
    opts.timeout_ms = 10000;
    auto f = engine.analyzeAsync("prompt", {}, "test", opts);
    f.get();
    
    ASSERT_TRUE(fakeClient->getLastOptions().timeout_ms.has_value());
    ASSERT_EQ(*fakeClient->getLastOptions().timeout_ms, 10000);
    
    // Test default (verifies the default argument = {} works)
    auto result2 = engine.analyze("prompt", {}, "test");
    (void)result2;
    ASSERT_FALSE(fakeClient->getLastOptions().timeout_ms.has_value());
}
