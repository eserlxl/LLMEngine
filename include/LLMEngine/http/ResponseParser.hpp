// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include "LLMEngine/LLMEngineExport.hpp"

#include <string>
#include <string_view>

namespace LLMEngine {

/**
 * @brief Parses LLM responses, extracting reasoning sections and content.
 *
 * Handles extraction of <think> tags and trims whitespace.
 * This service encapsulates response parsing logic for better testability.
 */
class LLMENGINE_EXPORT ResponseParser {
  public:
    /**
     * @brief Parse a response string into think section and content.
     *
     * Extracts content between <think> tags as the think section,
     * and the remaining content as the main response. Both are trimmed.
     *
     * @param response Full response from the LLM
     * @return Pair of (think_section, content_section), both trimmed
     *
     * @example
     * ```cpp
     * std::string response = "<think>I need to analyze this code</think>"
     *                        "The code has a bug in the loop condition.";
     * auto [think, content] = ResponseParser::parseResponse(response);
     * // think == "I need to analyze this code"
     * // content == "The code has a bug in the loop condition."
     * ```
     *
     * @example
     * ```cpp
     * std::string response = "No reasoning tags, just content.";
     * auto [think, content] = ResponseParser::parseResponse(response);
     * // think == ""
     * // content == "No reasoning tags, just content."
     * ```
     */
    [[nodiscard]] static std::pair<std::string, std::string> parseResponse(
        std::string_view response);

  private:
    /**
     * @brief Trim whitespace from a string.
     */
    [[nodiscard]] static std::string trim(const std::string& s);
};

} // namespace LLMEngine
