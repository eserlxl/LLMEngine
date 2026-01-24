#include "LLMEngine/LLMEngine.hpp"
#include "support/FakeAPIClient.hpp"
#include <cassert>
#include <future>
#include <iostream>
#include <thread>
#include <vector>

void testConcurrentAnalyze() {
    auto fake_client = std::make_unique<LLMEngineAPI::FakeAPIClient>();
    nlohmann::json params = {{"temperature", 0.7}, {"max_tokens", 100}};
    
    // Fully qualify to avoid namespace/class ambiguity
    ::LLMEngine::LLMEngine engine(std::move(fake_client), params);

    constexpr int num_threads = 20;
    std::vector<std::future<::LLMEngine::AnalysisResult>> futures;
    futures.reserve(num_threads);

    for (int i = 0; i < num_threads; ++i) {
        futures.push_back(std::async(std::launch::async, [&engine, i]() -> ::LLMEngine::AnalysisResult {
            nlohmann::json input;
            input["user_message"] = "Message " + std::to_string(i);
            // Disable terse instruction (false) to get exact prompt echo
            return engine.analyze("Test prompt", input, "concurrency_test", "chat", false);
        }));
    }

    for (auto& f : futures) {
        auto result = f.get();
        assert(result.success);
        assert(result.content == "[FAKE] Test prompt");
    }
    std::cout << "testConcurrentAnalyze passed" << std::endl;
}

void testConcurrentAnalyzeAsync() {
    auto fake_client = std::make_unique<LLMEngineAPI::FakeAPIClient>();
    nlohmann::json params = {{"temperature", 0.7}, {"max_tokens", 100}};
    ::LLMEngine::LLMEngine engine(std::move(fake_client), params);

    constexpr int num_threads = 20;
    std::vector<std::future<std::future<::LLMEngine::AnalysisResult>>> outer_futures;
    outer_futures.reserve(num_threads);

    for (int i = 0; i < num_threads; ++i) {
        outer_futures.push_back(std::async(std::launch::async, [&engine, i]() -> std::future<::LLMEngine::AnalysisResult> {
            nlohmann::json input;
            input["user_message"] = "Message " + std::to_string(i);
            // Disable terse instruction (false) to get exact prompt echo
            return engine.analyzeAsync("Test prompt", input, "concurrency_async_test", "chat", false);
        }));
    }

    for (auto& f : outer_futures) {
        auto inner_future = f.get();
        auto result = inner_future.get();
        assert(result.success);
        assert(result.content == "[FAKE] Test prompt");
    }
    std::cout << "testConcurrentAnalyzeAsync passed" << std::endl;
}

int main() {
    try {
        testConcurrentAnalyze();
        testConcurrentAnalyzeAsync();
        std::cout << "All tests passed" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}
