// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "Utils.hpp"
#include "LLMEngine.hpp"
#include <fstream>
#include <sstream>
#include <vector>
#include <cstdio>
#include <array>
#include <regex>
#include <iostream>
#include <filesystem>
#include <map>

namespace Utils {
    std::string TMP_DIR = "/var/log/lxl/llm";
    namespace fs = std::filesystem;

    std::vector<std::string> readLines(const std::string& filepath, size_t max_lines) {
        std::vector<std::string> lines;
        std::ifstream file(filepath);
        std::string line;
        while (std::getline(file, line) && lines.size() < max_lines) {
            lines.push_back(line);
        }
        return lines;
    }

    std::vector<std::string> execCommand(const std::string& cmd) {
        std::vector<std::string> output;
        std::array<char, 256> buffer;

        // Redirect stderr to stdout to capture errors
        std::string full_cmd = cmd + " 2>&1";
        FILE* pipe = popen(full_cmd.c_str(), "r");

        if (!pipe) {
            std::cerr << "[ERROR] popen() failed for command: " << cmd << std::endl;
            return output;
        }

        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            output.emplace_back(buffer.data());
        }

        int status = pclose(pipe);
        if (status != 0) {
            std::cerr << "[WARNING] Command '" << cmd << "' exited with non-zero status: " << status << std::endl;
            if (!output.empty()) {
                std::cerr << "  Output:" << std::endl;
                for (const auto& line : output) {
                    std::cerr << "    " << line;
                }
            }
        }
        return output;
    }

    std::string stripMarkdown(const std::string& input) {
        std::string output = input;
        output = std::regex_replace(output, std::regex(R"(\*\*)"), "");
        output = std::regex_replace(output, std::regex(R"(#+\s*)"), "");
        return output;
    }
} 