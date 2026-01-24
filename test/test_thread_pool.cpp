#include "LLMEngine/Utils/ThreadPool.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <atomic>
#include <chrono>
#include <thread>

int main() {
    std::cout << "Testing ThreadPool...\n";

    // Simple Task
    {
        LLMEngine::Utils::ThreadPool pool(4);
        auto result = pool.enqueue([] { return 42; });
        assert(result.get() == 42);
    }

    // Multiple Tasks
    {
        LLMEngine::Utils::ThreadPool pool(4);
        std::vector<std::future<int>> results;
        for(int i=0; i<8; ++i) {
            results.emplace_back(pool.enqueue([i] { return i * i; }));
        }
        for(int i=0; i<8; ++i) {
            assert(results[i].get() == i * i);
        }
    }

    // Destructor join
    {
        std::atomic<int> counter{0};
        {
            LLMEngine::Utils::ThreadPool pool(4);
            for(int i=0; i<100; ++i) {
                pool.enqueue([&counter] {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    counter++;
                });
            }
        }
        assert(counter == 100);
    }

    std::cout << "ThreadPool tests passed.\n";
    return 0;
}
