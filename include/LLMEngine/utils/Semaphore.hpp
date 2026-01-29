#pragma once
#include <condition_variable>
#include <mutex>

namespace LLMEngine {

/**
 * @brief Simple Semaphore for bounding concurrency.
 */
class Semaphore {
  public:
    explicit Semaphore(size_t count = 0) : count_(count) {}

    void acquire() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]() { return count_ > 0; });
        --count_;
    }

    void release() {
        std::lock_guard<std::mutex> lock(mutex_);
        ++count_;
        cv_.notify_one();
    }

  private:
    std::mutex mutex_;
    std::condition_variable cv_;
    size_t count_;
};

/**
 * @brief IDisposable-like guard for Semaphore.
 */
class SemaphoreGuard {
  public:
    explicit SemaphoreGuard(Semaphore& sem) : sem_(sem) {
        sem_.acquire();
    }
    ~SemaphoreGuard() {
        sem_.release();
    }
    // Delete copy/move
    SemaphoreGuard(const SemaphoreGuard&) = delete;
    SemaphoreGuard& operator=(const SemaphoreGuard&) = delete;

  private:
    Semaphore& sem_;
};

} // namespace LLMEngine
