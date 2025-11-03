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

namespace Utils {
    /**
     * @brief Directory for temporary artifacts.
     */
    extern std::string TMP_DIR;

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
     * - Allowed: alphanumeric, single spaces (no newlines/tabs), hyphens, underscores, dots, forward slashes
     * - Rejected: all control characters (newlines, tabs, carriage returns, etc.)
     * - Rejected: shell metacharacters (|, &, ;, $, `, <, >, parentheses, brackets, wildcards, etc.)
     * - Rejected: multiple consecutive spaces
     * 
     * The command string is split on spaces to create the argument vector. Only simple
     * commands with space-separated arguments are supported (no shell features).
     * 
     * @param cmd Command string to execute (e.g., "ls -la /tmp") - must be trusted/validated
     * @return Vector of output lines (stdout and stderr merged)
     */
    [[nodiscard]] std::vector<std::string> execCommand(std::string_view cmd);

    /**
     * @brief Remove Markdown syntax from input string.
     */
    [[nodiscard]] std::string stripMarkdown(std::string_view input);
}