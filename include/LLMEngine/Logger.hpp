// Copyright © 2025 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <memory>
#include <string_view>
#include <iostream>
#include <mutex>
#include "LLMEngine/LLMEngineExport.hpp"

namespace LLMEngine {

enum class LogLevel { Debug, Info, Warn, Error };

struct LLMENGINE_EXPORT Logger {
	virtual ~Logger() = default;
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
			case LogLevel::Debug: std::cout << "[DEBUG] " << message << '\n'; break;
			case LogLevel::Info:  std::cout << "[INFO] "  << message << '\n'; break;
			case LogLevel::Warn:  std::cerr << "[WARNING] " << message << '\n'; break;
			case LogLevel::Error: std::cerr << "[ERROR] " << message << '\n'; break;
		}
	}

private:
	mutable std::mutex mutex_;  ///< Mutex protecting stream writes
};

} // namespace LLMEngine


