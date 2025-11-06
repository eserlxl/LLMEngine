// Copyright © 2025 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "LLMEngine/LLMEngineExport.hpp"
#include <iostream>
#include <memory>
#include <mutex>
#include <string_view>

namespace LLMEngine {

enum class LogLevel { Debug, Info, Warn, Error };

/**
 * @brief Abstract logging interface.
 *
 * ## Thread Safety Requirements
 *
 * **Logger implementations MUST be thread-safe** if they will be shared across
 * multiple threads or used concurrently. The log() method may be called from
 * multiple threads simultaneously.
 *
 * ## Ownership
 *
 * Loggers are typically owned via shared_ptr to allow sharing across multiple
 * components. The lifetime is managed by the owner(s).
 */
struct LLMENGINE_EXPORT Logger {
    virtual ~Logger() = default;

    /**
     * @brief Log a message at the specified level.
     *
     * **Thread Safety:** This method MUST be thread-safe if the logger will be
     * used from multiple threads. Implementations should use appropriate
     * synchronization (mutexes, atomic operations, etc.).
     *
     * @param level Log level (Debug, Info, Warn, Error)
     * @param message Message to log
     */
    virtual void log(LogLevel level, std::string_view message) = 0;
};

/**
 * @brief Default logger implementation that writes to stdout/stderr.
 *
 * ## Thread Safety
 *
 * **DefaultLogger is thread-safe.** Uses a mutex to protect stream writes,
 * ensuring that log messages from multiple threads do not interleave.
 *
 * ## Usage
 *
 * ```cpp
 * auto logger = std::make_shared<DefaultLogger>();
 * engine.setLogger(logger);  // Can be shared across multiple instances
 * ```
 *
 * **Output:**
 * - Debug/Info: stdout with [DEBUG] or [INFO] prefix
 * - Warn/Error: stderr with [WARNING] or [ERROR] prefix
 */
class LLMENGINE_EXPORT DefaultLogger : public Logger {
public:
    /**
     * @brief Log a message (thread-safe implementation).
     *
     * Protected by mutex to ensure thread-safe stream writes.
     */
    void log(LogLevel level, std::string_view message) override {
        std::lock_guard<std::mutex> lock(mutex_);
        switch (level) {
            case LogLevel::Debug:
                std::cout << "[DEBUG] " << message << '\n';
                break;
            case LogLevel::Info:
                std::cout << "[INFO] " << message << '\n';
                break;
            case LogLevel::Warn:
                std::cerr << "[WARNING] " << message << '\n';
                break;
            case LogLevel::Error:
                std::cerr << "[ERROR] " << message << '\n';
                break;
        }
    }

private:
    mutable std::mutex mutex_; ///< Mutex protecting stream writes
};

} // namespace LLMEngine
