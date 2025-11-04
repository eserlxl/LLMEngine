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

class LLMENGINE_EXPORT DefaultLogger : public Logger {
public:
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
	mutable std::mutex mutex_;
};

} // namespace LLMEngine


