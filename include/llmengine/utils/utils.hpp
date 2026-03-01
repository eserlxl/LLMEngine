// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <cstdint>

// Forward declaration
namespace LLMEngine {
struct Logger;
}

namespace LLMEngine {
namespace Utils {


constexpr size_t defaultMaxLines = 100;

/**
 * @brief Read up to maxLines from a file.
 */
[[nodiscard]] std::vector<std::string> readLines(std::string_view filepath, size_t maxLines = defaultMaxLines);



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
 * @param apiKey API key to validate
 * @return true if API key appears valid, false otherwise
 *
 * @example
 * ```cpp
 * if (!Utils::validateApiKey(apiKey)) {
 *     throw std::invalid_argument("Invalid API key format");
 * }
 * ```
 */
[[nodiscard]] bool validateApiKey(std::string_view apiKey);

/**
 * @brief Validate model name format.
 *
 * Performs basic validation on model names:
 * - Non-empty
 * - Reasonable length (at most 256 characters)
 * - Only alphanumeric, hyphens, underscores, dots, and slashes
 * - No control characters
 *
 * @param modelName Model name to validate
 * @return true if model name appears valid, false otherwise
 *
 * @example
 * ```cpp
 * if (!Utils::validateModelName(model)) {
 *     throw std::invalid_argument("Invalid model name format");
 * }
 * ```
 */
[[nodiscard]] bool validateModelName(std::string_view modelName);

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

/**
 * @brief Encode data to Base64 string.
 */
[[nodiscard]] std::string base64Encode(std::span<const uint8_t> data);

/**
 * @brief Encode string to Base64 string.
 */
[[nodiscard]] std::string base64Encode(std::string_view data);
} // namespace Utils
} // namespace LLMEngine
