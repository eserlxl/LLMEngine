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
    std::string TMP_DIR = "/tmp/llmengine";
    namespace fs = std::filesystem;

    // Constants
    constexpr size_t COMMAND_BUFFER_SIZE = 256;

    std::vector<std::string> readLines(std::string_view filepath, size_t max_lines) {
        std::vector<std::string> lines;
        std::ifstream file{std::string(filepath)};
        std::string line;
        while (std::getline(file, line) && lines.size() < max_lines) {
            lines.push_back(line);
        }
        return lines;
    }

    std::vector<std::string> execCommand(std::string_view cmd) {
        std::vector<std::string> output;
        std::array<char, COMMAND_BUFFER_SIZE> buffer;

        // SECURITY: Validate command string to prevent command injection
        // Only allow alphanumeric, spaces, hyphens, underscores, dots, slashes, and common safe characters
        // This is a whitelist approach - if your use case needs special characters, validate them explicitly
        const std::regex safe_chars(R"([a-zA-Z0-9_./\s-]+)");
        std::string cmd_str(cmd);
        
        if (cmd_str.empty()) {
            std::cerr << "[ERROR] execCommand: Empty command string" << std::endl;
            return output;
        }
        
        // Check if command contains potentially dangerous characters
        if (!std::regex_match(cmd_str, safe_chars)) {
            std::cerr << "[ERROR] execCommand: Command contains potentially unsafe characters: " << cmd_str << std::endl;
            std::cerr << "[ERROR] Only alphanumeric, spaces, hyphens, underscores, dots, and slashes are allowed" << std::endl;
            std::cerr << "[ERROR] For security reasons, shell metacharacters (|, &, ;, $, `, etc.) are not permitted" << std::endl;
            return output;
        }
        
        // Additional check: prevent command chaining attempts
        if (cmd_str.find('|') != std::string::npos ||
            cmd_str.find('&') != std::string::npos ||
            cmd_str.find(';') != std::string::npos ||
            cmd_str.find('$') != std::string::npos ||
            cmd_str.find('`') != std::string::npos ||
            cmd_str.find('(') != std::string::npos ||
            cmd_str.find(')') != std::string::npos ||
            cmd_str.find('<') != std::string::npos ||
            cmd_str.find('>') != std::string::npos) {
            std::cerr << "[ERROR] execCommand: Command contains shell metacharacters - rejected for security" << std::endl;
            return output;
        }

        // Redirect stderr to stdout to capture errors
        std::string full_cmd = cmd_str + " 2>&1";
        FILE* pipe = popen(full_cmd.c_str(), "r");

        if (!pipe) {
            std::cerr << "[ERROR] popen() failed for command: " << cmd_str << std::endl;
            return output;
        }

        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            output.emplace_back(buffer.data());
        }

        int status = pclose(pipe);
        if (status != 0) {
            std::cerr << "[WARNING] Command '" << cmd_str << "' exited with non-zero status: " << status << std::endl;
            if (!output.empty()) {
                std::cerr << "  Output:" << std::endl;
                for (const auto& line : output) {
                    std::cerr << "    " << line;
                }
            }
        }
        return output;
    }

    std::string stripMarkdown(std::string_view input) {
        std::string output = std::string(input);
        output = std::regex_replace(output, std::regex(R"(\*\*)"), "");
        output = std::regex_replace(output, std::regex(R"(#+\s*)"), "");
        return output;
    }
} 