// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include <string>
#include <string_view>
#include <vector>

// Forward declaration
namespace LLMEngine {
struct Logger;
}

namespace LLMEngine {
namespace Utils {
/**
 * @brief Default directory for temporary artifacts.
 *
 * @deprecated Use LLMEngine::ITempDirProvider for thread-safe temporary directory management.
 * This constant is provided for backwards compatibility only.
 */
constexpr const char* TMP_DIR = "/tmp/llmengine";

/**
 * @brief Read up to max_lines from a file.
 */
[[nodiscard]] std::vector<std::string> readLines(std::string_view filepath, size_t max_lines = 100);

/**
 * @brief Execute a command and capture stdout/stderr lines.
 *
 * SECURITY: This function uses posix_spawn() which does NOT route through a shell,
 * eliminating shell injection risks. The command string is parsed into an argv array
 * and executed directly, bypassing shell interpretation entirely.
 *
 * Validation rules:
 * - Allowed: alphanumeric, single spaces (no newlines/tabs), hyphens, underscores, dots, forward
 * slashes
 * - Rejected: all control characters (newlines, tabs, carriage returns, etc.)
 * - Rejected: shell metacharacters (|, &, ;, $, `, <, >, parentheses, brackets, wildcards, etc.)
 * - Rejected: multiple consecutive spaces
 *
 * The command string is split on spaces to create the argument vector. Only simple
 * commands with space-separated arguments are supported (no shell features).
 *
 * @param cmd Command string to execute (e.g., "ls -la /tmp") - must be trusted/validated
 * @param logger Optional logger for error messages (nullptr to suppress)
 * @return Vector of output lines (stdout and stderr merged)
 */
[[nodiscard]] std::vector<std::string> execCommand(std::string_view cmd,
                                                   ::LLMEngine::Logger* logger = nullptr);

/**
 * @brief Execute a command with pre-parsed arguments (bypasses shell parsing entirely).
 *
 * This overload accepts a vector of arguments directly, completely bypassing shell
 * parsing and validation. Use this for trusted inputs where you have full control
 * over the argument list (e.g., system utilities, tooling scenarios).
 *
 * SECURITY: This function uses posix_spawn() which does NOT route through a shell.
 * No validation is performed on arguments - they are passed directly to execvp().
 * Only use this with trusted, well-formed argument vectors.
 *
 * @param args Vector of arguments (first element is the program, subsequent are arguments)
 *             Example: {"ls", "-la", "/tmp"}
 * @param logger Optional logger for error messages (nullptr to suppress)
 * @return Vector of output lines (stdout and stderr merged)
 */
[[nodiscard]] std::vector<std::string> execCommand(const std::vector<std::string>& args,
                                                   ::LLMEngine::Logger* logger = nullptr);

/**
 * @brief Remove Markdown syntax from input string.
 */
[[nodiscard]] std::string stripMarkdown(std::string_view input);

/**
 * @brief Validate API key format (basic checks).
 *
 * Performs basic validation on API keys:
 * - Non-empty
 * - Reasonable length (at least 10 characters, at most 512 characters)
 * - No control characters
 *
 * @param api_key API key to validate
 * @return true if API key appears valid, false otherwise
 *
 * @example
 * ```cpp
 * if (!Utils::validateApiKey(api_key)) {
 *     throw std::invalid_argument("Invalid API key format");
 * }
 * ```
 */
[[nodiscard]] bool validateApiKey(std::string_view api_key);

/**
 * @brief Validate model name format.
 *
 * Performs basic validation on model names:
 * - Non-empty
 * - Reasonable length (at most 256 characters)
 * - Only alphanumeric, hyphens, underscores, dots, and slashes
 * - No control characters
 *
 * @param model_name Model name to validate
 * @return true if model name appears valid, false otherwise
 *
 * @example
 * ```cpp
 * if (!Utils::validateModelName(model)) {
 *     throw std::invalid_argument("Invalid model name format");
 * }
 * ```
 */
[[nodiscard]] bool validateModelName(std::string_view model_name);

/**
 * @brief Validate URL format (basic checks).
 *
 * Performs basic validation on URLs:
 * - Non-empty
 * - Starts with http:// or https://
 * - Reasonable length (at most 2048 characters)
 * - No control characters
 *
 * @param url URL to validate
 * @return true if URL appears valid, false otherwise
 *
 * @example
 * ```cpp
 * if (!Utils::validateUrl(base_url)) {
 *     throw std::invalid_argument("Invalid URL format");
 * }
 * ```
 */
[[nodiscard]] bool validateUrl(std::string_view url);
} // namespace Utils
} // namespace LLMEngine
