// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LLMEngine/LLMEngine.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

using namespace LLMEngine;

// Simple test runner for async lifetime
int main() {
    std::cout << "Testing Async Operation Survives Engine Destruction..." << std::endl;

    std::future<AnalysisResult> future_result;

    try {
        {
            std::cout << "Creating engine..." << std::endl;
            // Explicit namespace qualification
            auto engine = std::make_unique<LLMEngine::LLMEngine>(
                LLMEngineAPI::ProviderType::OLLAMA, "", "llama3", nlohmann::json{}, 24, false);

            std::cout << "Starting async analysis..." << std::endl;
            future_result = engine->analyzeAsync("Can you see me?", nlohmann::json{}, "test_type");

            std::cout << "Destroying engine..." << std::endl;
            // Engine is destroyed here
        }

        std::cout << "Engine destroyed. Waiting for future..." << std::endl;

        if (!future_result.valid()) {
            std::cerr << "Future is invalid!" << std::endl;
            return 1;
        }

        try {
            auto result = future_result.get();
            std::cout << "Future retrieved result (success=" << result.success << ")" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Future threw exception (expected if no backend): " << e.what()
                      << std::endl;
        }

        std::cout << "Test passed: no crash." << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception." << std::endl;
        return 1;
    }
}
