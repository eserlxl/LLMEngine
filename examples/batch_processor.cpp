// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LLMEngine/LLMEngine.hpp"
#include "LLMEngine/RequestContext.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <vector>

using namespace LLMEngine;

// specialized interceptor for logging
class BatchLogger : public LLMEngine::LLMEngine::IInterceptor {
  public:
    void onRequest(RequestContext& /*ctx*/) override {
        // Simple logging
        // std::cout << "[Interceptor] Starting request: " << ctx.analysisType << std::endl;
    }

    void onResponse(AnalysisResult& /*result*/) override {
        // std::cout << "[Interceptor] Finished request. Success: " << result.success << std::endl;
    }
};

void printUsage() {
    std::cout << "Usage: ./batch_processor <provider> <api_key> [count] [concurrency]" << std::endl;
    std::cout << "Example: ./batch_processor qwen YOUR_KEY 10 4" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printUsage();
        return 1;
    }

    std::string provider = argv[1];
    std::string apiKey = argv[2];
    int count = (argc > 3) ? std::stoi(argv[3]) : 5;
    int concurrency = (argc > 4) ? std::stoi(argv[4]) : 2;

    std::cout << "🚀 Starting Batch Processor" << std::endl;
    std::cout << "Provider: " << provider << std::endl;
    std::cout << "Requests: " << count << std::endl;
    std::cout << "Concurrency: " << concurrency << std::endl;

    try {
        // Initialize Engine
        LLMEngine::LLMEngine engine(provider, apiKey, "", {}, 24, false);

        // Add Interceptor
        engine.addInterceptor(std::make_shared<BatchLogger>());

        // Prepare Inputs
        std::vector<AnalysisInput> inputs;
        inputs.reserve(count);
        for (int i = 0; i < count; ++i) {
            inputs.push_back(
                AnalysisInput::builder()
                    .withSystemPrompt("You remain brief.")
                    .withUserMessage("Say hello and your number is " + std::to_string(i)));
        }

        // Configure Options
        RequestOptions options;
        options.max_concurrency = concurrency;
        options.timeout_ms = 10000; // 10s timeout

        auto start = std::chrono::high_resolution_clock::now();

        // Execute Batch
        std::cout << "\nRunning batch..." << std::endl;
        auto results = engine.analyzeBatch(inputs, "batch_test", options);

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;

        // Process Results
        int success = 0;
        for (const auto& res : results) {
            if (res.success) {
                success++;
                // std::cout << "  -> " << res.content << std::endl;
            } else {
                std::cerr << "  -> Fallied: " << res.errorMessage << std::endl;
            }
        }

        std::cout << "\n✅ Batch Complete!" << std::endl;
        std::cout << "Time: " << std::fixed << std::setprecision(2) << elapsed.count() << "s"
                  << std::endl;
        std::cout << "Success Rate: " << success << "/" << count << std::endl;

        // Simple heuristic check for concurrency
        // If 10 requests took < (10 * avg_latency / concurrency), it worked.

    } catch (const std::exception& e) {
        std::cerr << "❌ Fatal Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
