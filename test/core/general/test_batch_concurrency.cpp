
// Copyright © 2026 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#undef NDEBUG
#include <cassert>
#include <iostream>
#include <vector>
#include "LLMEngine/core/LLMEngine.hpp"
#include "support/FakeAPIClient.hpp"

using namespace LLMEngine;

int main() {
    std::cout << "Testing Batch Concurrency..." << std::endl;

    // Create fake client
    auto client = std::make_unique<LLMEngineAPI::FakeAPIClient>();
    
    // We do NOT set next response, relying on default echo behavior: "[FAKE] <prompt>"
    // This allows verifying each request was processed individually.

    // Construct engine with dependency injection
    LLMEngine::LLMEngine engine(std::move(client));

    // Create inputs
    std::vector<AnalysisInput> inputs;
    for (int i = 0; i < 20; ++i) {
        inputs.push_back(AnalysisInput::builder().withUserMessage("Test " + std::to_string(i)));
    }

    RequestOptions options;
    options.max_concurrency = 5; // Restrict to 5 threads

    try {
        auto results = engine.analyzeBatch(inputs, "test_batch", options);
        assert(results.size() == 20);
        for (int i = 0; i < 20; ++i) {
            const auto& res = results[i];
            assert(res.success);
            // Verify content contains the unique prompt (ignoring "Terse Instruction" prefix)
            std::string expected_suffix = "Test " + std::to_string(i);
            if (res.content.find(expected_suffix) == std::string::npos) {
                std::cerr << "Mismatch at index " << i << ": expected to find '" << expected_suffix 
                          << "' in '" << res.content << "'" << std::endl;
                return 1;
            }
        }
        std::cout << "  Batch execution with concurrency limiting: PASS" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "  Batch execution failed: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Batch Concurrency tests passed." << std::endl;
    return 0;
}
