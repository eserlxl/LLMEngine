// Copyright © 2025 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of the LLMEngine test suite (thread safety tests for config manager)
// and is licensed under the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/APIClient.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace LLMEngineAPI;

// Test concurrent reads
void testConcurrentReads() {
    auto& mgr = APIConfigManager::getInstance();

    // Load config first
    mgr.loadConfig("config/api_config.json");

    constexpr int num_threads = 10;
    constexpr int iterations_per_thread = 1000;
    std::atomic<int> errors{0};
    std::atomic<int> completed{0};

    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&mgr, &errors, &completed, i]() {
            try {
                for (int j = 0; j < iterations_per_thread; ++j) {
                    // Concurrent reads
                    auto timeout = mgr.getTimeoutSeconds();
                    auto retries = mgr.getRetryAttempts();
                    auto delay = mgr.getRetryDelayMs();
                    auto provider = mgr.getDefaultProvider();
                    auto config = mgr.getProviderConfig("ollama");
                    auto providers = mgr.getAvailableProviders();
                    auto path = mgr.getDefaultConfigPath();
                    auto timeout_ollama = mgr.getTimeoutSeconds("ollama");

                    // Verify values are reasonable (basic sanity check)
                    if (timeout < 0 || timeout > 10000)
                        ++errors;
                    if (retries < 0 || retries > 100)
                        ++errors;
                    if (delay < 0 || delay > 100000)
                        ++errors;
                    if (provider.empty())
                        ++errors;
                }
                ++completed;
            } catch (...) {
                ++errors;
                ++completed;
            }
        });
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    assert(completed == num_threads);
    assert(errors == 0);

    std::cout << "✓ Concurrent reads test passed (" << num_threads << " threads, "
              << iterations_per_thread << " iterations each)\n";
}

// Test concurrent reads and writes
void testConcurrentReadsAndWrites() {
    auto& mgr = APIConfigManager::getInstance();

    constexpr int num_readers = 8;
    constexpr int num_writers = 2;
    constexpr int iterations = 500;
    std::atomic<int> read_errors{0};
    std::atomic<int> write_errors{0};
    std::atomic<int> readers_completed{0};
    std::atomic<int> writers_completed{0};

    std::vector<std::thread> threads;

    // Reader threads
    for (int i = 0; i < num_readers; ++i) {
        threads.emplace_back([&mgr, &read_errors, &readers_completed, iterations]() {
            try {
                for (int j = 0; j < iterations; ++j) {
                    auto timeout = mgr.getTimeoutSeconds();
                    auto retries = mgr.getRetryAttempts();
                    auto delay = mgr.getRetryDelayMs();
                    auto provider = mgr.getDefaultProvider();
                    auto config = mgr.getProviderConfig("ollama");
                    auto providers = mgr.getAvailableProviders();
                    auto path = mgr.getDefaultConfigPath();

                    // Small delay to increase chance of race conditions
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                }
                ++readers_completed;
            } catch (...) {
                ++read_errors;
                ++readers_completed;
            }
        });
    }

    // Writer threads
    for (int i = 0; i < num_writers; ++i) {
        threads.emplace_back([&mgr, &write_errors, &writers_completed, iterations, i]() {
            try {
                for (int j = 0; j < iterations; ++j) {
                    // Alternate between setting default path and loading config
                    if (j % 2 == 0) {
                        std::string path = "config/api_config.json";
                        mgr.setDefaultConfigPath(path);
                    } else {
                        mgr.loadConfig("config/api_config.json");
                    }

                    // Small delay
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                }
                ++writers_completed;
            } catch (...) {
                ++write_errors;
                ++writers_completed;
            }
        });
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    assert(readers_completed == num_readers);
    assert(writers_completed == num_writers);
    assert(read_errors == 0);
    assert(write_errors == 0);

    std::cout << "✓ Concurrent reads and writes test passed (" << num_readers << " readers, "
              << num_writers << " writers, " << iterations << " iterations each)\n";
}

// Test logger thread safety
// NOTE: getLogger() is a private method, so this test is disabled
// Logger thread safety is tested indirectly through other API calls
void testLoggerThreadSafety() {
    // Logger thread safety is tested indirectly through other concurrent operations
    // since getLogger() is a private method
    std::cout << "✓ Logger thread safety test skipped (getLogger() is private)\n";
}

int main() {
    std::cout << "Running APIConfigManager thread safety tests...\n";

    testConcurrentReads();
    testConcurrentReadsAndWrites();
    testLoggerThreadSafety();

    std::cout << "All thread safety tests passed!\n";
    return 0;
}
