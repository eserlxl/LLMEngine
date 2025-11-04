// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include <string_view>
#include "LLMEngine/LLMEngineExport.hpp"

namespace LLMEngine {

/**
 * @brief Centralized constants for JSON keys, environment variables, and filenames.
 * 
 * This namespace contains all hardcoded string literals used throughout the library
 * to reduce duplication and prevent typos.
 */
namespace Constants {

// JSON Configuration Keys
namespace JsonKeys {
    constexpr std::string_view API_KEY = "api_key";
    constexpr std::string_view DEFAULT_MODEL = "default_model";
    constexpr std::string_view BASE_URL = "base_url";
    constexpr std::string_view DEFAULT_PROVIDER = "default_provider";
    constexpr std::string_view TIMEOUT_SECONDS = "timeout_seconds";
    constexpr std::string_view RETRY_ATTEMPTS = "retry_attempts";
    constexpr std::string_view RETRY_DELAY_MS = "retry_delay_ms";
    constexpr std::string_view RETRY_MAX_DELAY_MS = "retry_max_delay_ms";
    constexpr std::string_view RETRY_EXPONENTIAL = "retry_exponential";
    constexpr std::string_view JITTER_SEED = "jitter_seed";
    constexpr std::string_view PROVIDERS = "providers";
    constexpr std::string_view MAX_TOKENS = "max_tokens";
    constexpr std::string_view MODE = "mode";
    constexpr std::string_view DEFAULT_PARAMS = "default_params";
    constexpr std::string_view SYSTEM_PROMPT = "system_prompt";
    constexpr std::string_view TEMPERATURE = "temperature";
    constexpr std::string_view TOP_P = "top_p";
    constexpr std::string_view TOP_K = "top_k";
    constexpr std::string_view MIN_P = "min_p";
    constexpr std::string_view FREQUENCY_PENALTY = "frequency_penalty";
    constexpr std::string_view PRESENCE_PENALTY = "presence_penalty";
} // namespace JsonKeys

// Environment Variable Names
namespace EnvVars {
    constexpr std::string_view QWEN_API_KEY = "QWEN_API_KEY";
    constexpr std::string_view OPENAI_API_KEY = "OPENAI_API_KEY";
    constexpr std::string_view ANTHROPIC_API_KEY = "ANTHROPIC_API_KEY";
    constexpr std::string_view GEMINI_API_KEY = "GEMINI_API_KEY";
    constexpr std::string_view DISABLE_DEBUG_FILES = "LLMENGINE_DISABLE_DEBUG_FILES";
    constexpr std::string_view LOG_REQUESTS = "LLMENGINE_LOG_REQUESTS";
} // namespace EnvVars

// Debug Artifact Filenames
namespace DebugFiles {
    constexpr std::string_view API_RESPONSE_JSON = "api_response.json";
    constexpr std::string_view API_RESPONSE_ERROR_JSON = "api_response_error.json";
    constexpr std::string_view RESPONSE_FULL_TXT = "response_full.txt";
    constexpr std::string_view THINK_TXT_SUFFIX = ".think.txt";
    constexpr std::string_view TXT_SUFFIX = ".txt";
    constexpr std::string_view TMP_SUFFIX = ".tmp";
} // namespace DebugFiles

// Default URLs
namespace DefaultUrls {
    constexpr std::string_view OLLAMA_BASE = "http://localhost:11434";
    constexpr std::string_view QWEN_BASE = "https://dashscope-intl.aliyuncs.com/compatible-mode/v1";
    constexpr std::string_view OPENAI_BASE = "https://api.openai.com/v1";
    constexpr std::string_view ANTHROPIC_BASE = "https://api.anthropic.com/v1";
    constexpr std::string_view GEMINI_BASE = "https://generativelanguage.googleapis.com/v1";
} // namespace DefaultUrls

// Default Models
namespace DefaultModels {
    constexpr std::string_view QWEN = "qwen-flash";
    constexpr std::string_view OPENAI = "gpt-3.5-turbo";
    constexpr std::string_view ANTHROPIC = "claude-3-sonnet";
    constexpr std::string_view GEMINI = "gemini-1.5-flash";
    constexpr std::string_view OLLAMA = "llama2";
} // namespace DefaultModels

// Default Configuration Path
namespace ConfigPaths {
    constexpr std::string_view DEFAULT_CONFIG = "config/api_config.json";
    constexpr std::string_view INSTALL_CONFIG = "/usr/share/llmEngine/config/api_config.json";
} // namespace ConfigPaths

// Request Directory Prefix
namespace RequestDirs {
    constexpr std::string_view PREFIX = "req_";
} // namespace RequestDirs

} // namespace Constants

} // namespace LLMEngine

