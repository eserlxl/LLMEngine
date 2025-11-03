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
#include <cctype>
#include <array>
#include <regex>
#include <iostream>
#include <filesystem>
#include <map>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <algorithm>

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
        std::string cmd_str(cmd);

        // SECURITY: Validate command string to prevent command injection
        // This function uses posix_spawn() which does NOT route through a shell,
        // eliminating shell injection risks. However, we still validate input to ensure
        // only safe command strings are accepted.
        // Only allow alphanumeric, single spaces (not newlines/tabs), hyphens, underscores, dots, slashes
        const std::regex safe_chars(R"([a-zA-Z0-9_./ -]+)");
        
        if (cmd_str.empty()) {
            std::cerr << "[ERROR] execCommand: Empty command string" << std::endl;
            return output;
        }
        
        // Explicitly reject control characters including newlines, tabs, carriage returns
        // These can be used for command injection even with metacharacter checks
        for (char c : cmd_str) {
            if (std::iscntrl(static_cast<unsigned char>(c))) {
                std::cerr << "[ERROR] execCommand: Command contains control characters (newlines, tabs, etc.) - rejected for security" << std::endl;
                return output;
            }
        }
        
        // Explicitly check for newlines and tabs (redundant but defensive)
        if (cmd_str.find('\n') != std::string::npos ||
            cmd_str.find('\r') != std::string::npos ||
            cmd_str.find('\t') != std::string::npos) {
            std::cerr << "[ERROR] execCommand: Command contains newlines, carriage returns, or tabs - rejected for security" << std::endl;
            return output;
        }
        
        // Check if command matches whitelist pattern (no \s, only explicit space)
        if (!std::regex_match(cmd_str, safe_chars)) {
            std::cerr << "[ERROR] execCommand: Command contains potentially unsafe characters: " << cmd_str << std::endl;
            std::cerr << "[ERROR] Only alphanumeric, single spaces, hyphens, underscores, dots, and slashes are allowed" << std::endl;
            std::cerr << "[ERROR] For security reasons, shell metacharacters and control characters are not permitted" << std::endl;
            return output;
        }
        
        // Additional check: prevent command chaining attempts via shell metacharacters
        if (cmd_str.find('|') != std::string::npos ||
            cmd_str.find('&') != std::string::npos ||
            cmd_str.find(';') != std::string::npos ||
            cmd_str.find('$') != std::string::npos ||
            cmd_str.find('`') != std::string::npos ||
            cmd_str.find('(') != std::string::npos ||
            cmd_str.find(')') != std::string::npos ||
            cmd_str.find('<') != std::string::npos ||
            cmd_str.find('>') != std::string::npos ||
            cmd_str.find('{') != std::string::npos ||
            cmd_str.find('}') != std::string::npos ||
            cmd_str.find('[') != std::string::npos ||
            cmd_str.find(']') != std::string::npos ||
            cmd_str.find('*') != std::string::npos ||
            cmd_str.find('?') != std::string::npos ||
            cmd_str.find('!') != std::string::npos ||
            cmd_str.find('#') != std::string::npos ||
            cmd_str.find('~') != std::string::npos) {
            std::cerr << "[ERROR] execCommand: Command contains shell metacharacters - rejected for security" << std::endl;
            return output;
        }
        
        // Prevent multiple consecutive spaces (could be used to hide malicious content)
        if (cmd_str.find("  ") != std::string::npos) {
            std::cerr << "[ERROR] execCommand: Command contains multiple consecutive spaces - rejected for security" << std::endl;
            return output;
        }

        // Parse command string into argv array (split on spaces)
        std::vector<std::string> args;
        std::istringstream iss(cmd_str);
        std::string arg;
        while (iss >> arg) {
            args.push_back(arg);
        }
        
        if (args.empty()) {
            std::cerr << "[ERROR] execCommand: No command found in: " << cmd_str << std::endl;
            return output;
        }

        // Prepare argv array for execve (must be null-terminated)
        std::vector<char*> argv;
        argv.reserve(args.size() + 1);  // Pre-allocate capacity for args + nullptr
        for (auto& arg_str : args) {
            argv.push_back(const_cast<char*>(arg_str.c_str()));
        }
        argv.push_back(nullptr);

        // Create pipes for stdout and stderr
        std::array<int, 2> stdout_pipe;
        std::array<int, 2> stderr_pipe;
        if (pipe(stdout_pipe.data()) != 0 || pipe(stderr_pipe.data()) != 0) {
            std::cerr << "[ERROR] execCommand: Failed to create pipes for command: " << cmd_str << std::endl;
            return output;
        }

        // Set up file actions for posix_spawn
        posix_spawn_file_actions_t file_actions;
        posix_spawn_file_actions_init(&file_actions);
        posix_spawn_file_actions_addclose(&file_actions, stdout_pipe[0]);
        posix_spawn_file_actions_addclose(&file_actions, stderr_pipe[0]);
        posix_spawn_file_actions_adddup2(&file_actions, stdout_pipe[1], STDOUT_FILENO);
        posix_spawn_file_actions_adddup2(&file_actions, stderr_pipe[1], STDERR_FILENO);
        posix_spawn_file_actions_addclose(&file_actions, stdout_pipe[1]);
        posix_spawn_file_actions_addclose(&file_actions, stderr_pipe[1]);

        // Spawn the process
        pid_t pid;
        extern char** environ;  // Use current environment
        int spawn_result = posix_spawnp(&pid, argv[0], &file_actions, nullptr, argv.data(), environ);

        // Clean up file actions
        posix_spawn_file_actions_destroy(&file_actions);

        if (spawn_result != 0) {
            std::cerr << "[ERROR] execCommand: posix_spawnp() failed for command: " << cmd_str << " (error: " << spawn_result << ")" << std::endl;
            close(stdout_pipe[0]);
            close(stdout_pipe[1]);
            close(stderr_pipe[0]);
            close(stderr_pipe[1]);
            return output;
        }

        // Close write ends of pipes in parent
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        // Read stdout and stderr
        // Note: We read both sequentially. In practice, commands typically write to one or the other,
        // and this approach matches the original popen() behavior (which merged via 2>&1)
        std::array<char, COMMAND_BUFFER_SIZE> buffer;
        ssize_t bytes_read;
        
        // Read stdout
        std::string stdout_buffer;
        while ((bytes_read = read(stdout_pipe[0], buffer.data(), buffer.size())) > 0) {
            stdout_buffer.append(buffer.data(), static_cast<size_t>(bytes_read));
        }
        
        // Read stderr
        std::string stderr_buffer;
        while ((bytes_read = read(stderr_pipe[0], buffer.data(), buffer.size())) > 0) {
            stderr_buffer.append(buffer.data(), static_cast<size_t>(bytes_read));
        }
        
        // Process stdout line by line (preserving original fgets behavior)
        std::istringstream stdout_stream(stdout_buffer);
        std::string line;
        while (std::getline(stdout_stream, line)) {
            output.push_back(line + "\n");
        }
        
        // Process stderr line by line and append
        std::istringstream stderr_stream(stderr_buffer);
        while (std::getline(stderr_stream, line)) {
            output.push_back(line + "\n");
        }

        // Close read ends
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);

        // Wait for process to complete
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            std::cerr << "[ERROR] execCommand: waitpid() failed for command: " << cmd_str << std::endl;
            return output;
        }

        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            std::cerr << "[WARNING] Command '" << cmd_str << "' exited with non-zero status: " << WEXITSTATUS(status) << std::endl;
            if (!output.empty()) {
                std::cerr << "  Output:" << std::endl;
                for (const auto& output_line : output) {
                    std::cerr << "    " << output_line;
                }
            }
        } else if (WIFSIGNALED(status)) {
            std::cerr << "[WARNING] Command '" << cmd_str << "' terminated by signal: " << WTERMSIG(status) << std::endl;
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