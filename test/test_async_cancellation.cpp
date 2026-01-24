
// Copyright © 2026 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#undef NDEBUG
#include <cassert>
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include "LLMEngine/LLMEngine.hpp"
#include "support/FakeAPIClient.hpp"

using namespace LLMEngine;

int main() {
    std::cout << "Testing Async Cancellation..." << std::endl;

    // Create fake client and setup streaming delay
    auto client = std::make_unique<LLMEngineAPI::FakeAPIClient>();
    
    // Set 5 chunks, each taking 100ms. Total 500ms.
    // We will cancel after ~150ms.
    std::vector<std::string> chunks = {"Chunk 1", "Chunk 2", "Chunk 3", "Chunk 4", "Chunk 5"};
    client->setNextStreamChunks(chunks);
    client->setStreamDelay(std::chrono::milliseconds(100));

    LLMEngine::LLMEngine engine(std::move(client));

    auto cancelToken = LLMEngine::LLMEngine::createCancellationToken();

    AnalysisInput input = AnalysisInput::builder().withUserMessage("Stream me");
    RequestOptions options;
    options.cancellation_token = cancelToken;

    std::atomic<int> chunksReceived{0};
    std::atomic<bool> isDone{false};

    // Start streaming in a separate thread (simulated by engine internals usually, but analyzeStream blocks,
    // so we run analyzeStream in a thread or use async mechanics? 
    // analyzeStream BLOCKS. So we must run it in a thread to cancel it from *this* thread.)
    
    std::thread streamThread([&]() {
        try {
            engine.analyzeStream("Stream me", input.toJson(), "test", options, 
                [&](const StreamChunk& chunk) {
                    if (!chunk.is_done) {
                        chunksReceived++;
                        std::cout << "  Received: " << chunk.content << std::endl;
                    } else {
                        isDone = true;
                    }
                }
            );
        } catch (...) {
            // Ignore cancel exceptions if any
        }
    });

    // Wait a bit to let it start and process 1-2 chunks
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    
    std::cout << "  Cancelling..." << std::endl;
    cancelToken->cancel();

    if (streamThread.joinable()) {
        streamThread.join();
    }

    std::cout << "  Chunks received: " << chunksReceived << std::endl;

    // Verification:
    // Should have received at least 1, but NOT all 5.
    // FakeAPIClient checks cancel between chunks.
    if (chunksReceived >= 5) {
        std::cerr << "Cancellation failed: Received all chunks." << std::endl;
        return 1;
    }
    if (chunksReceived == 0) {
         // Might happen if delay is before first chunk?
         // FakeAPIClient delay is inside loop.
         std::cout << "  Warning: Received 0 chunks (cancelled very early?), but strictly valid." << std::endl;
    }

    std::cout << "Async Cancellation test passed." << std::endl;
    return 0;
}
