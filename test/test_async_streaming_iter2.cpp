// Copyright © 2026 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LLMEngine/LLMEngine.hpp"
#include "support/FakeAPIClient.hpp"
#include <chrono>
#include <future>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

using LLMEngine::AnalysisResult;

void testAsync() {
    std::cout << "Testing analyzeAsync..." << std::endl;
    // Create engine with Fake client
    auto fakeClient = std::make_unique<LLMEngineAPI::FakeAPIClient>();

    // We can't easily mock delay in FakeAPIClient without modification,
    // but we can verify the future works.

    ::LLMEngine::LLMEngine engine(std::move(fakeClient));

    std::cout << "Launching async task..." << std::endl;
    nlohmann::json input;
    std::future<AnalysisResult> future = engine.analyzeAsync("Async prompt", input, "async_test");

    if (!future.valid()) {
        throw std::runtime_error("Future is invalid immediately after launch");
    }

    std::cout << "Waiting for future..." << std::endl;
    AnalysisResult result = future.get();

    if (!result.success) {
        throw std::runtime_error("Async result failed: " + result.errorMessage);
    }
    if (result.content.find("[FAKE]") == std::string::npos) {
        throw std::runtime_error("Unexpected content: " + result.content);
    }

    std::cout << "analyzeAsync passed." << std::endl;
}

void testStreaming() {
    std::cout << "Testing analyzeStream..." << std::endl;
    auto fakeClient = std::make_unique<LLMEngineAPI::FakeAPIClient>();
    fakeClient->setNextStreamChunks({"Chunk 1", " ", "Chunk 2", " ", "Done"});

    ::LLMEngine::LLMEngine engine(std::move(fakeClient));

    std::vector<std::string> received;
    std::cout << "Starting stream..." << std::endl;

    ::LLMEngine::RequestOptions options;
    engine.analyzeStream(
        "Stream prompt", {}, "stream_test", options, [&](const LLMEngine::StreamChunk& chunk) {
            if (!chunk.content.empty()) {
                received.push_back(std::string(chunk.content));
                std::cout << "Rx: " << chunk.content << std::endl;
            }
            if (chunk.is_done)
                std::cout << "Stream done signal." << std::endl;
        });

    // We expect 5 text chunks + potentially empty "done" calls depending on implementation
    // LLMEngine calls callback("", true) at end.

    std::string full_text;
    for (const auto& s : received)
        full_text += s;

    if (full_text != "Chunk 1 Chunk 2 Done") {
        throw std::runtime_error("Stream content mismatch. Got: '" + full_text + "'");
    }

    std::cout << "analyzeStream passed." << std::endl;
}

int main() {
    try {
        testAsync();
        std::cout << "----------------" << std::endl;
        testStreaming();
        std::cout << "All Iteration 2 tests passed." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}
