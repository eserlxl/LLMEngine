// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of the LLMEngine test suite (thread safety tests for RequestContextBuilder)
// and is licensed under the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/IModelContext.hpp"
#include "LLMEngine/Logger.hpp"
#include "LLMEngine/RequestContextBuilder.hpp"
#include "LLMEngine/TempDirectoryService.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

using namespace LLMEngine;

// Mock IModelContext for testing
class MockModelContext : public IModelContext {
  public:
    MockModelContext() : temp_dir_("/tmp/llmengine_test_rng") {
        // Ensure test directory exists
        std::filesystem::create_directories(temp_dir_);
    }

    ~MockModelContext() {
        // Cleanup test directory
        std::error_code ec;
        std::filesystem::remove_all(temp_dir_, ec);
    }

    std::string getTempDirectory() const override {
        return temp_dir_;
    }
    std::shared_ptr<IPromptBuilder> getTersePromptBuilder() const override {
        return nullptr;
    }
    std::shared_ptr<IPromptBuilder> getPassthroughPromptBuilder() const override {
        return nullptr;
    }
    const nlohmann::json& getModelParams() const override {
        return model_params_;
    }
    bool areDebugFilesEnabled() const override {
        return false;
    }
    std::shared_ptr<IArtifactSink> getArtifactSink() const override {
        return nullptr;
    }
    int getLogRetentionHours() const override {
        return 24;
    }
    std::shared_ptr<Logger> getLogger() const override {
        return nullptr;
    }
    void prepareTempDirectory() const override {}

  private:
    std::string temp_dir_;
    nlohmann::json model_params_;
};

// Test concurrent directory name generation to ensure uniqueness
void testConcurrentDirectoryGeneration() {
    MockModelContext context;
    constexpr int num_threads = 20;
    constexpr int iterations_per_thread = 100;
    [[maybe_unused]] constexpr int total_requests = num_threads * iterations_per_thread;

    std::atomic<int> completed{0};
    std::atomic<int> errors{0};
    std::mutex dir_names_mutex;
    std::unordered_set<std::string> generated_dirs;

    std::vector<std::thread> threads;

    auto start_time = std::chrono::steady_clock::now();

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(
            [&context, &completed, &errors, &dir_names_mutex, &generated_dirs, i]() {
                try {
                    for (int j = 0; j < iterations_per_thread; ++j) {
                        nlohmann::json input;
                        RequestContext ctx = RequestContextBuilder::build(
                            context, "test prompt", input, "test_analysis", "chat", false);

                        // Verify directory name is unique
                        std::lock_guard<std::mutex> lock(dir_names_mutex);
                        if (generated_dirs.find(ctx.requestTmpDir) != generated_dirs.end()) {
                            std::cerr
                                << "ERROR: Duplicate directory name detected: " << ctx.requestTmpDir
                                << " (thread " << i << ", iteration " << j << ")\n";
                            ++errors;
                        } else {
                            generated_dirs.insert(ctx.requestTmpDir);
                        }
                    }
                    ++completed;
                } catch (const std::exception& e) {
                    std::cerr << "ERROR in thread " << i << ": " << e.what() << "\n";
                    ++errors;
                    ++completed;
                } catch (...) {
                    std::cerr << "ERROR in thread " << i << ": Unknown exception\n";
                    ++errors;
                    ++completed;
                }
            });
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Verify all threads completed
    assert(completed == num_threads);
    assert(errors == 0);
    assert(static_cast<int>(generated_dirs.size()) == total_requests);

    std::cout << "✓ Concurrent directory generation test passed:\n";
    std::cout << "  - Threads: " << num_threads << "\n";
    std::cout << "  - Iterations per thread: " << iterations_per_thread << "\n";
    std::cout << "  - Total unique directories: " << generated_dirs.size() << "\n";
    std::cout << "  - Duration: " << duration.count() << " ms\n";
}

// Test rapid concurrent initialization (stress test for RNG initialization)
void testRapidConcurrentInitialization() {
    MockModelContext context;
    constexpr int num_threads = 50;
    constexpr int iterations_per_thread = 10;

    std::atomic<int> completed{0};
    std::atomic<int> errors{0};
    std::mutex dir_names_mutex;
    std::unordered_set<std::string> generated_dirs;

    std::vector<std::thread> threads;

    // Create all threads at once to maximize concurrent RNG initialization
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&context, &completed, &errors, &dir_names_mutex, &generated_dirs]() {
            try {
                for (int j = 0; j < iterations_per_thread; ++j) {
                    nlohmann::json input;
                    RequestContext ctx =
                        RequestContextBuilder::build(context, "test", input, "test", "chat", false);

                    std::lock_guard<std::mutex> lock(dir_names_mutex);
                    if (generated_dirs.find(ctx.requestTmpDir) != generated_dirs.end()) {
                        ++errors;
                    } else {
                        generated_dirs.insert(ctx.requestTmpDir);
                    }
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
    assert(static_cast<int>(generated_dirs.size()) == num_threads * iterations_per_thread);

    std::cout << "✓ Rapid concurrent initialization test passed:\n";
    std::cout << "  - Threads: " << num_threads << "\n";
    std::cout << "  - Unique directories: " << generated_dirs.size() << "\n";
}

int main() {
    std::cout << "Running RequestContextBuilder thread safety tests...\n\n";

    try {
        testConcurrentDirectoryGeneration();
        std::cout << "\n";
        testRapidConcurrentInitialization();
        std::cout << "\n";
        std::cout << "All thread safety tests passed!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception\n";
        return 1;
    }
}
