// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include "llmengine/LLMEngineExport.hpp"

#include <string_view>

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
constexpr std::string_view apiKey = "api_key";
constexpr std::string_view defaultModel = "default_model";
constexpr std::string_view baseUrl = "base_url";
constexpr std::string_view defaultProvider = "default_provider";
constexpr std::string_view timeoutSeconds = "timeout_seconds";
constexpr std::string_view retryAttempts = "retry_attempts";
constexpr std::string_view retryDelayMs = "retry_delay_ms";
constexpr std::string_view retryMaxDelayMs = "retry_max_delay_ms";
constexpr std::string_view retryExponential = "retry_exponential";
constexpr std::string_view jitterSeed = "jitter_seed";
constexpr std::string_view providers = "providers";
constexpr std::string_view maxTokens = "max_tokens";
constexpr std::string_view mode = "mode";
constexpr std::string_view defaultParams = "default_params";
constexpr std::string_view systemPrompt = "system_prompt";
constexpr std::string_view temperature = "temperature";
constexpr std::string_view topP = "top_p";
constexpr std::string_view topK = "top_k";
constexpr std::string_view minP = "min_p";

constexpr std::string_view logprobs = "logprobs";
constexpr std::string_view topLogprobs = "top_logprobs";
constexpr std::string_view user = "user";
constexpr std::string_view frequencyPenalty = "frequency_penalty";
constexpr std::string_view presencePenalty = "presence_penalty";
} // namespace JsonKeys

// Environment Variable Names
namespace EnvVars {
constexpr std::string_view qwenApiKey = "qwenApiKey";
constexpr std::string_view openaiApiKey = "openaiApiKey";
constexpr std::string_view anthropicApiKey = "anthropicApiKey";
constexpr std::string_view geminiApiKey = "geminiApiKey";

// Base URLs
constexpr std::string_view ollamaHost = "ollamaHost"; // Standard Ollama env var
constexpr std::string_view openaiBaseUrl = "openaiBaseUrl";
constexpr std::string_view qwenBaseUrl = "qwenBaseUrl";           // Not standard but useful
constexpr std::string_view anthropicBaseUrl = "anthropicBaseUrl"; // Not standard but useful
constexpr std::string_view geminiBaseUrl = "geminiBaseUrl";       // Not standard but useful

// Models
constexpr std::string_view defaultModel = "defaultModel"; // Global override
constexpr std::string_view ollamaModel = "ollamaModel";
constexpr std::string_view openaiModel = "openaiModel";
constexpr std::string_view qwenModel = "qwenModel";
constexpr std::string_view anthropicModel = "anthropicModel";
constexpr std::string_view geminiModel = "geminiModel";

constexpr std::string_view disableDebugFiles = "LLMENGINE_DISABLE_DEBUG_FILES";
constexpr std::string_view logRequests = "LLMENGINE_LOG_REQUESTS";
} // namespace EnvVars

// Debug Artifact Filenames (aliased for backward compatibility)
namespace DebugFiles {
constexpr std::string_view apiResponseJson = "api_response.json";
constexpr std::string_view apiResponseErrorJson = "api_response_error.json";
constexpr std::string_view responseFullTxt = "response_full.txt";
constexpr std::string_view thinkTxtSuffix = ".think.txt";
constexpr std::string_view txtSuffix = ".txt";
constexpr std::string_view tmpSuffix = ".tmp";
} // namespace DebugFiles

// Debug Artifact constants (preferred namespace)
namespace DebugArtifacts {
constexpr std::string_view apiResponseJson = "api_response.json";
constexpr std::string_view apiResponseErrorJson = "api_response_error.json";
constexpr std::string_view responseFullTxt = "response_full.txt";
constexpr std::string_view thinkTxtSuffix = ".think.txt";
constexpr std::string_view contentTxtSuffix = ".txt";
constexpr std::string_view redactedReasoningTagOpen = "<think>";
constexpr std::string_view redactedReasoningTagClose = "</think>";
} // namespace DebugArtifacts

// Default URLs
namespace DefaultUrls {
constexpr std::string_view ollamaBase = "http://localhost:11434";
constexpr std::string_view qwenBase = "https://dashscope-intl.aliyuncs.com/compatible-mode/v1";
constexpr std::string_view openaiBase = "https://api.openai.com/v1";
constexpr std::string_view anthropicBase = "https://api.anthropic.com/v1";
constexpr std::string_view geminiBase = "https://generativelanguage.googleapis.com/v1";
} // namespace DefaultUrls

// Default Models
namespace DefaultModels {
constexpr std::string_view qwen = "qwen-flash";
constexpr std::string_view openai = "gpt-3.5-turbo";
constexpr std::string_view anthropic = "claude-3-sonnet";
constexpr std::string_view gemini = "gemini-1.5-flash";
constexpr std::string_view ollama = "llama2";
} // namespace DefaultModels

// Default Configuration Path
namespace ConfigPaths {
constexpr std::string_view defaultConfig = "config/api_config.json";
constexpr std::string_view installConfig = "/usr/share/llmEngine/config/api_config.json";
} // namespace ConfigPaths

// File Paths (for backward compatibility)
namespace FilePaths {
constexpr std::string_view defaultConfigPath = "config/api_config.json";
constexpr std::string_view tmpDir = "/tmp/llmengine";
} // namespace FilePaths

// Default Values
namespace DefaultValues {
constexpr int timeoutSeconds = 30;
constexpr int ollamaTimeoutSeconds = 300; // 5 minutes for Ollama
constexpr int maxTimeoutSeconds = 600;    // 10 minutes maximum timeout
constexpr int retryAttempts = 3;
constexpr int retryDelayMs = 1000;
constexpr int maxBackoffDelayMs = 30000;
constexpr double temperature = 0.7;
constexpr int maxTokens = 2000;
constexpr double topP = 0.9;
constexpr double minP = 0.05;
constexpr int topK = 40;
constexpr int contextWindow = 10000;
constexpr int defaultLogRetentionHours = 24;
constexpr int millisecondsPerSecond = 1000;
constexpr int maxConnectTimeoutMs = 120000; // 2 minutes
} // namespace DefaultValues

// Request Directory Prefix
namespace RequestDirs {
constexpr std::string_view prefix = "req_";
} // namespace RequestDirs

} // namespace Constants

} // namespace LLMEngine
