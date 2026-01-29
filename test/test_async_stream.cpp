// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LLMEngine/LLMEngine.hpp"
#include "support/FakeAPIClient.hpp"
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace LLMEngineAPI;

#define ASSERT_TRUE(cond)                                                                          \
    if (!(cond)) {                                                                                 \
        std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << #cond        \
                  << std::endl;                                                                    \
        throw std::runtime_error("Assertion failed");                                              \
    }

#define ASSERT_EQ(a, b)                                                                            \
    if ((a) != (b)) {                                                                              \
        std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << #a           \
                  << " == " << #b << " (Values: " << (a) << " vs " << (b) << ")" << std::endl;     \
        throw std::runtime_error("Assertion failed");                                              \
    }

void testAnalyzeAsyncReturnsFuture() {
    std::cout << "Testing AnalyzeAsyncReturnsFuture..." << std::endl;
    auto client = std::make_unique<FakeAPIClient>();
    auto* fake_client = client.get();

    ::LLMEngine::LLMEngine engine(std::move(client));

    fake_client->setNextResponse({true, "Async Response", "", 200, {}, {}, {}, ""});

    nlohmann::json input;
    auto future = engine.analyzeAsync("test prompt", input, "test_async");

    ASSERT_TRUE(future.valid());

    auto result = future.get();
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.content, std::string("Async Response"));
    std::cout << "PASS" << std::endl;
}

void testAnalyzeStreamCallsCallback() {
    std::cout << "Testing AnalyzeStreamCallsCallback..." << std::endl;
    auto client = std::make_unique<FakeAPIClient>();
    auto* fake_client = client.get();

    ::LLMEngine::LLMEngine engine(std::move(client));

    std::vector<std::string> chunks = {"Chunk1", "Chunk2", "Chunk3"};
    fake_client->setNextStreamChunks(chunks);

    std::vector<std::string> received_chunks;
    bool received_done = false;

    nlohmann::json input;
    ::LLMEngine::RequestOptions options;
    engine.analyzeStream(
        "test stream", input, "test_stream", options, [&](const LLMEngine::StreamChunk& chunk) {
            if (!chunk.is_done) {
                received_chunks.emplace_back(chunk.content);
            } else {
                received_done = true;
            }
        });

    ASSERT_EQ(received_chunks.size(), chunks.size());
    for (size_t i = 0; i < chunks.size(); ++i) {
        ASSERT_EQ(received_chunks[i], chunks[i]);
    }
    ASSERT_TRUE(received_done);
    std::cout << "PASS" << std::endl;
}

void testAnalyzeStreamDefaultsWithoutChunks() {
    std::cout << "Testing AnalyzeStreamDefaultsWithoutChunks..." << std::endl;
    auto client = std::make_unique<FakeAPIClient>();

    ::LLMEngine::LLMEngine engine(std::move(client));
    // No chunks set, defaults to echo

    std::vector<std::string> received_chunks;
    bool received_done = false;

    nlohmann::json input;
    ::LLMEngine::RequestOptions options;
    engine.analyzeStream("test default",
                         input,
                         "test_stream_default",
                         options,
                         [&](const LLMEngine::StreamChunk& chunk) {
                             if (!chunk.is_done) {
                                 received_chunks.emplace_back(chunk.content);
                             } else {
                                 received_done = true;
                             }
                         });

    ASSERT_EQ(received_chunks.size(), 1UL);
    ASSERT_TRUE(received_chunks[0].find("[FAKE STREAM]") != std::string::npos);
    ASSERT_TRUE(received_done);
    std::cout << "PASS" << std::endl;
}

int main() {
    try {
        testAnalyzeAsyncReturnsFuture();
        testAnalyzeStreamCallsCallback();
        testAnalyzeStreamDefaultsWithoutChunks();
        std::cout << "All async/stream tests passed." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
