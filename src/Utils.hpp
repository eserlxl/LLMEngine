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
    extern std::string TMP_DIR;
    std::vector<std::string> readLines(const std::string& filepath, size_t max_lines = 100);
    std::vector<std::string> execCommand(const std::string& cmd);
    std::string stripMarkdown(const std::string& input);
} 