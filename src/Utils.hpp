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
#include "LLMEngine.hpp"

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
     * @brief Execute a shell command and capture stdout lines.
     * 
     * SECURITY WARNING: This function uses popen() which routes through a shell.
     * While validation prevents most injection vectors, it should NEVER be used with
     * untrusted input. Only use with hardcoded commands or commands that have been
     * explicitly validated and sanitized by the caller.
     * 
     * Validation rules:
     * - Allowed: alphanumeric, single spaces (no newlines/tabs), hyphens, underscores, dots, forward slashes
     * - Rejected: all control characters (newlines, tabs, carriage returns, etc.)
     * - Rejected: shell metacharacters (|, &, ;, $, `, <, >, parentheses, brackets, wildcards, etc.)
     * - Rejected: multiple consecutive spaces
     * 
     * NOTE: For production use with untrusted input, consider using posix_spawn/execve
     * with argv arrays instead of this shell-based approach.
     * 
     * @param cmd Command string to execute (must be trusted/validated)
     * @return Vector of output lines
     */
    [[nodiscard]] std::vector<std::string> execCommand(std::string_view cmd);

    /**
     * @brief Remove Markdown syntax from input string.
     */
    [[nodiscard]] std::string stripMarkdown(std::string_view input);
}