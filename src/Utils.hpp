// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include <string>
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
    std::vector<std::string> readLines(const std::string& filepath, size_t max_lines = 100);

    /**
     * @brief Execute a shell command and capture stdout lines.
     */
    std::vector<std::string> execCommand(const std::string& cmd);

    /**
     * @brief Remove Markdown syntax from input string.
     */
    std::string stripMarkdown(const std::string& input);
}