// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/Utils.hpp"
#include "LLMEngine/Logger.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <ranges>
#include <regex>
#include <sstream>
#include <vector>

// POSIX-specific includes (not available on Windows)
#if defined(__unix__) || defined(__unix) ||                                        \
    (defined(__APPLE__) && defined(__MACH__))
#include <spawn.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <unistd.h>
#elif defined(_WIN32) || defined(_WIN64)
#include <io.h>
#endif

namespace LLMEngine::Utils {
namespace fs = std::filesystem;

// Constants
constexpr size_t COMMAND_BUFFER_SIZE = 256;
constexpr size_t MAX_OUTPUT_LINES =
    10000; // Maximum number of output lines to prevent memory exhaustion
constexpr size_t MAX_LINE_LENGTH =
    static_cast<size_t>(1024) * 1024; // Maximum line length (1MB) to prevent
                                      // memory exhaustion
constexpr size_t MAX_CMD_STRING_LENGTH = 4096; // Maximum allowed command string
                                                // length
constexpr size_t MAX_ARG_COUNT = 64;           // Maximum number of arguments
constexpr size_t MAX_ARG_LENGTH = 512;         // Maximum length per argument

// Validation constants
constexpr size_t MIN_API_KEY_LENGTH = 10;
constexpr size_t MAX_API_KEY_LENGTH = 512;
constexpr size_t MAX_MODEL_NAME_LENGTH = 256;
constexpr size_t MAX_URL_LENGTH = 2048;
constexpr size_t MIN_URL_LENGTH = 7;      // Minimum "http://"
constexpr size_t HTTP_PREFIX_LENGTH = 7;  // "http://"
constexpr size_t HTTPS_PREFIX_LENGTH = 8; // "https://"

// Precompiled regex patterns
static const std::regex SAFE_CHARS_REGEX(R"([a-zA-Z0-9_./ -]+)");
static const std::regex MARKDOWN_BOLD_REGEX(R"(\*\*)");
static const std::regex MARKDOWN_HEADER_REGEX(R"(#+\s*)");

// Platform-specific close helper
namespace {
void close_fd(int fd) {
#if defined(__unix__) || defined(__unix) ||                                        \
    (defined(__APPLE__) && defined(__MACH__))
    close(fd);
#elif defined(_WIN32) || defined(_WIN64)
    _close(fd);
#endif
}
} // namespace

// RAII wrapper for pipe file descriptors
class PipeFD {
public:
    explicit PipeFD(int fd) : fd_(fd) {}
    ~PipeFD() {
        if (fd_ >= 0) {
            close_fd(fd_);
        }
    }
    PipeFD(const PipeFD&) = delete;
    PipeFD& operator=(const PipeFD&) = delete;
    PipeFD(PipeFD&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }
    PipeFD& operator=(PipeFD&& other) noexcept {
        if (this != &other) {
            if (fd_ >= 0)
                close_fd(fd_);
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }
    [[nodiscard]] int get() const {
        return fd_;
    }
    int release() {
        int fd = fd_;
        fd_ = -1;
        return fd;
    }

private:
    int fd_;
};

std::vector<std::string> readLines(std::string_view filepath, size_t max_lines) {
    std::vector<std::string> lines;
    lines.reserve(max_lines);
    std::ifstream file{std::string(filepath)};
    std::string line;
    while (std::getline(file, line) && lines.size() < max_lines) {
        lines.push_back(line);
    }
    return lines;
}

// Internal helper function to execute command with pre-parsed arguments
// This is shared by both execCommand overloads
// SECURITY: Extensive validation is performed here to prevent command injection:
// - Argument count and length limits
// - Regex-based character whitelist (alphanumeric, spaces, hyphens, underscores,
//   dots, slashes)
// - Control character rejection (newlines, tabs, carriage returns)
// - Shell metacharacter rejection (|, &, ;, $, `, <, >, parentheses, brackets,
//   wildcards)
// - Multiple redundant checks for defense in depth
std::vector<std::string> execCommandImpl(
    const std::vector<std::string>& args, ::LLMEngine::Logger* logger,
    const std::string& cmd_str_for_logging) {
    std::vector<std::string> output;

    // Windows support: execCommand is not available on Windows due to POSIX
    // dependencies
#if defined(_WIN32) || defined(_WIN64)
    if (logger) {
        logger->log(::LLMEngine::LogLevel::Error,
                    "execCommand: Not available on Windows. This function "
                    "requires POSIX spawn API.");
    }
    return output;
#endif

    if (args.empty()) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Error,
                        "execCommand: Empty command string");
        }
        return output;
    }

    // Enforce input size caps even for the vector overload (defensive
    // programming)
    if (cmd_str_for_logging.size() > MAX_CMD_STRING_LENGTH) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Error,
                        "execCommand: Command string too long - rejected for "
                        "security");
        }
        return output;
    }
    if (args.size() > MAX_ARG_COUNT) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Error,
                        "execCommand: Too many arguments - rejected for security");
        }
        return output;
    }
    for (const auto& a : args) {
        if (a.size() > MAX_ARG_LENGTH || !std::regex_match(a, SAFE_CHARS_REGEX)) {
            if (logger) {
                logger->log(::LLMEngine::LogLevel::Error,
                            "execCommand: Argument validation failed - rejected "
                            "for security");
            }
            return output;
        }
    }

    // Explicitly reject control characters including newlines, tabs, carriage
    // returns These can be used for command injection even with metacharacter
    // checks
    for (char c : cmd_str_for_logging) {
        if (std::iscntrl(static_cast<unsigned char>(c))) {
            if (logger) {
                logger->log(::LLMEngine::LogLevel::Error,
                            "execCommand: Command contains control characters "
                            "(newlines, tabs, etc.) - rejected for security");
            }
            return output;
        }
    }

    // Explicitly check for newlines and tabs (redundant but defensive)
    if (cmd_str_for_logging.find('\n') != std::string::npos ||
        cmd_str_for_logging.find('\r') != std::string::npos ||
        cmd_str_for_logging.find('\t') != std::string::npos) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Error,
                        "execCommand: Command contains newlines, carriage "
                        "returns, or tabs - rejected for security");
        }
        return output;
    }

    // Check if command matches whitelist pattern (no \s, only explicit space)
    if (!std::regex_match(cmd_str_for_logging, SAFE_CHARS_REGEX)) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Error,
                        std::string("execCommand: Command contains potentially "
                                    "unsafe characters: ") +
                            cmd_str_for_logging);
            logger->log(::LLMEngine::LogLevel::Error,
                        "Only alphanumeric, single spaces, hyphens, underscores, "
                        "dots, and slashes are allowed");
            logger->log(::LLMEngine::LogLevel::Error,
                        "For security reasons, shell metacharacters and control "
                        "characters are not permitted");
        }
        return output;
    }

    // Additional check: prevent command chaining attempts via shell
    // metacharacters
    if (cmd_str_for_logging.find('|') != std::string::npos ||
        cmd_str_for_logging.find('&') != std::string::npos ||
        cmd_str_for_logging.find(';') != std::string::npos ||
        cmd_str_for_logging.find('$') != std::string::npos ||
        cmd_str_for_logging.find('`') != std::string::npos ||
        cmd_str_for_logging.find('(') != std::string::npos ||
        cmd_str_for_logging.find(')') != std::string::npos ||
        cmd_str_for_logging.find('<') != std::string::npos ||
        cmd_str_for_logging.find('>') != std::string::npos ||
        cmd_str_for_logging.find('{') != std::string::npos ||
        cmd_str_for_logging.find('}') != std::string::npos ||
        cmd_str_for_logging.find('[') != std::string::npos ||
        cmd_str_for_logging.find(']') != std::string::npos ||
        cmd_str_for_logging.find('*') != std::string::npos ||
        cmd_str_for_logging.find('?') != std::string::npos ||
        cmd_str_for_logging.find('!') != std::string::npos ||
        cmd_str_for_logging.find('#') != std::string::npos ||
        cmd_str_for_logging.find('~') != std::string::npos) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Error,
                        "execCommand: Command contains shell metacharacters - "
                        "rejected for security");
        }
        return output;
    }

    // Prevent multiple consecutive spaces (could be used to hide malicious
    // content)
    if (cmd_str_for_logging.find("  ") != std::string::npos) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Error,
                        "execCommand: Command contains multiple consecutive "
                        "spaces - rejected for security");
        }
        return output;
    }

    // Prepare argv array for execve (must be null-terminated)
    std::vector<char*> argv;
    argv.reserve(args.size() + 1); // Pre-allocate capacity for args + nullptr
    for (auto& arg_str : args) {
        argv.push_back(const_cast<char*>(arg_str.c_str()));
    }
    argv.push_back(nullptr);

    // Create pipes for stdout and stderr
    std::array<int, 2> stdout_pipe;
    std::array<int, 2> stderr_pipe;
    if (pipe(stdout_pipe.data()) != 0 || pipe(stderr_pipe.data()) != 0) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Error,
                        std::string("execCommand: Failed to create pipes for "
                                    "command: ") +
                            cmd_str_for_logging);
        }
        return output;
    }

    // RAII wrappers to ensure pipes are closed on all error paths
    PipeFD stdout_read(stdout_pipe[0]);
    PipeFD stdout_write(stdout_pipe[1]);
    PipeFD stderr_read(stderr_pipe[0]);
    PipeFD stderr_write(stderr_pipe[1]);

    // Set up file actions for posix_spawn
    posix_spawn_file_actions_t file_actions;
    if (posix_spawn_file_actions_init(&file_actions) != 0) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Error,
                        "execCommand: posix_spawn_file_actions_init failed");
        }
        return output;
    }
    auto add_action = [&](int rc, const char* what) -> bool {
        if (rc != 0) {
            if (logger) {
                logger->log(::LLMEngine::LogLevel::Error,
                            std::string("execCommand: ") + what + " failed (" +
                                std::to_string(rc) + ")");
            }
            posix_spawn_file_actions_destroy(&file_actions);
            return false;
        }
        return true;
    };
    if (!add_action(posix_spawn_file_actions_addclose(&file_actions, stdout_read.get()),
                    "addclose stdout_read"))
        return output;
    if (!add_action(posix_spawn_file_actions_addclose(&file_actions, stderr_read.get()),
                    "addclose stderr_read"))
        return output;
    if (!add_action(
            posix_spawn_file_actions_adddup2(&file_actions, stdout_write.get(),
                                             STDOUT_FILENO),
            "adddup2 stdout"))
        return output;
    if (!add_action(
            posix_spawn_file_actions_adddup2(&file_actions, stderr_write.get(),
                                             STDERR_FILENO),
            "adddup2 stderr"))
        return output;
    if (!add_action(posix_spawn_file_actions_addclose(&file_actions, stdout_write.get()),
                    "addclose stdout_write"))
        return output;
    if (!add_action(posix_spawn_file_actions_addclose(&file_actions, stderr_write.get()),
                    "addclose stderr_write"))
        return output;

    // Spawn the process
    pid_t pid;
    int spawn_result = posix_spawnp(&pid, argv[0], &file_actions, nullptr,
                                    argv.data(), ::environ);

    // Clean up file actions
    posix_spawn_file_actions_destroy(&file_actions);

    if (spawn_result != 0) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Error,
                        std::string("execCommand: posix_spawnp() failed for "
                                    "command: ") +
                            cmd_str_for_logging + " (error: " +
                            std::to_string(spawn_result) + ")");
        }
        // Pipes are automatically closed by RAII wrappers
        return output;
    }

    // Close write ends of pipes in parent (RAII will handle cleanup on return)
    stdout_write = PipeFD(-1);
    stderr_write = PipeFD(-1);

    // Read stdout and stderr concurrently to prevent deadlock when one pipe
    // fills up SECURITY: Enforce limits to prevent resource exhaustion from
    // user-controlled commands Use poll() to read from both pipes simultaneously,
    // avoiding blocking on one while the other fills up (which could cause the
    // child process to block)
    std::array<char, COMMAND_BUFFER_SIZE> buffer;
    std::string stdout_buffer;
    std::string stderr_buffer;
    size_t stdout_total_size = 0;
    size_t stderr_total_size = 0;

    // Set up poll structures for both pipes
    std::array<struct pollfd, 2> pollfds;
    pollfds[0].fd = stdout_read.get();
    pollfds[0].events = POLLIN;
    pollfds[1].fd = stderr_read.get();
    pollfds[1].events = POLLIN;

    bool stdout_closed = false;
    bool stderr_closed = false;

    // Poll and read from both pipes until both are closed
    while (!stdout_closed || !stderr_closed) {
        // Build poll array with only active fds
        std::vector<struct pollfd> active_pollfds;
        std::vector<int> fd_to_index; // Maps active_pollfds index to original
                                      // pollfds index
        if (!stdout_closed) {
            active_pollfds.push_back(pollfds[0]);
            fd_to_index.push_back(0);
        }
        if (!stderr_closed) {
            active_pollfds.push_back(pollfds[1]);
            fd_to_index.push_back(1);
        }

        if (active_pollfds.empty())
            break;

        int poll_result = poll(active_pollfds.data(), active_pollfds.size(), -1);
        if (poll_result < 0) {
            // poll() error - break and process what we have
            if (logger) {
                logger->log(::LLMEngine::LogLevel::Warn,
                            "execCommand: poll() failed, terminating read");
            }
            break;
        }

        // Update original pollfds with revents from active_pollfds
        for (size_t i = 0; i < active_pollfds.size(); ++i) {
            pollfds[static_cast<size_t>(fd_to_index[i])].revents = active_pollfds[i].revents;
        }

        // Read from stdout if available and not truncated
        if (!stdout_closed && (pollfds[0].revents & POLLIN)) {
            ssize_t bytes_read = read(stdout_read.get(), buffer.data(), buffer.size());
            if (bytes_read > 0) {
                stdout_total_size += static_cast<size_t>(bytes_read);
                if (stdout_total_size > MAX_LINE_LENGTH * MAX_OUTPUT_LINES) {
                    if (logger) {
                        logger->log(::LLMEngine::LogLevel::Warn,
                                    "execCommand: Output exceeds maximum size "
                                    "limit, truncating");
                    }
                    stdout_closed = true;
                } else {
                    stdout_buffer.append(buffer.data(), static_cast<size_t>(bytes_read));
                }
            } else if (bytes_read == 0 ||
                       (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                // EOF or error (not just would-block)
                stdout_closed = true;
                pollfds[0].fd = -1; // Remove from poll set
            }
        } else if (!stdout_closed && (pollfds[0].revents & (POLLHUP | POLLERR))) {
            stdout_closed = true;
            pollfds[0].fd = -1;
        }

        // Read from stderr if available and not truncated
        if (!stderr_closed && (pollfds[1].revents & POLLIN)) {
            ssize_t bytes_read = read(stderr_read.get(), buffer.data(), buffer.size());
            if (bytes_read > 0) {
                stderr_total_size += static_cast<size_t>(bytes_read);
                if (stderr_total_size > MAX_LINE_LENGTH * MAX_OUTPUT_LINES) {
                    if (logger) {
                        logger->log(::LLMEngine::LogLevel::Warn,
                                    "execCommand: Error output exceeds maximum "
                                    "size limit, truncating");
                    }
                    stderr_closed = true;
                } else {
                    stderr_buffer.append(buffer.data(), static_cast<size_t>(bytes_read));
                }
            } else if (bytes_read == 0 ||
                       (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                // EOF or error (not just would-block)
                stderr_closed = true;
                pollfds[1].fd = -1; // Remove from poll set
            }
        } else if (!stderr_closed && (pollfds[1].revents & (POLLHUP | POLLERR))) {
            stderr_closed = true;
            pollfds[1].fd = -1;
        }
    }

    size_t total_lines = 0;

    // Process stdout line by line with limits
    std::istringstream stdout_stream(stdout_buffer);
    std::string line;
    while (std::getline(stdout_stream, line) && total_lines < MAX_OUTPUT_LINES) {
        // Limit individual line length
        if (line.size() > MAX_LINE_LENGTH) {
            line.resize(MAX_LINE_LENGTH);
            if (logger) {
                logger->log(::LLMEngine::LogLevel::Warn,
                            "execCommand: Line truncated due to length limit");
            }
        }
        output.push_back(line);
        total_lines++;
    }
    if (total_lines >= MAX_OUTPUT_LINES) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Warn,
                        std::string("execCommand: Output truncated at ") +
                            std::to_string(MAX_OUTPUT_LINES) + " lines");
        }
    }

    // Process stderr line by line with limits
    std::istringstream stderr_stream(stderr_buffer);
    while (std::getline(stderr_stream, line) && total_lines < MAX_OUTPUT_LINES) {
        // Limit individual line length
        if (line.size() > MAX_LINE_LENGTH) {
            line.resize(MAX_LINE_LENGTH);
            if (logger) {
                logger->log(::LLMEngine::LogLevel::Warn,
                            "execCommand: Error line truncated due to length "
                            "limit");
            }
        }
        output.push_back(line);
        total_lines++;
    }
    if (total_lines >= MAX_OUTPUT_LINES) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Warn,
                        std::string("execCommand: Total output truncated at ") +
                            std::to_string(MAX_OUTPUT_LINES) + " lines");
        }
    }

    // Pipes are automatically closed by RAII wrappers

    // Wait for process to complete
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Error,
                        std::string("execCommand: waitpid() failed for command: ") +
                            cmd_str_for_logging);
        }
        return output;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Warn,
                        std::string("Command '") + cmd_str_for_logging +
                            "' exited with non-zero status: " +
                            std::to_string(WEXITSTATUS(status)));
            if (!output.empty()) {
                std::string output_msg = "  Output:\n";
                for (const auto& output_line : output) {
                    output_msg += "    " + output_line + "\n";
                }
                logger->log(::LLMEngine::LogLevel::Warn, output_msg);
            }
        }
    } else if (WIFSIGNALED(status)) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Warn,
                        std::string("Command '") + cmd_str_for_logging +
                            "' terminated by signal: " +
                            std::to_string(WTERMSIG(status)));
        }
    }

    return output;
}

std::vector<std::string> execCommand(std::string_view cmd, ::LLMEngine::Logger* logger) {
    std::vector<std::string> output;

    // Windows support: execCommand is not available on Windows due to POSIX
    // dependencies
#if defined(_WIN32) || defined(_WIN64)
    if (logger) {
        logger->log(::LLMEngine::LogLevel::Error,
                    "execCommand: Not available on Windows. This function "
                    "requires POSIX spawn API.");
    }
    return output;
#endif

    std::string cmd_str(cmd);

    // SECURITY: Validate command string to prevent command injection
    // This function uses posix_spawn() which does NOT route through a shell,
    // eliminating shell injection risks. However, we still validate input to
    // ensure only safe command strings are accepted.
    // Only allow alphanumeric, single spaces (not newlines/tabs), hyphens,
    // underscores, dots, slashes

    if (cmd_str.empty()) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Error,
                        "execCommand: Empty command string");
        }
        return output;
    }

    if (cmd_str.size() > MAX_CMD_STRING_LENGTH) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Error,
                        "execCommand: Command string too long - rejected for "
                        "security");
        }
        return output;
    }

    // Explicitly reject control characters including newlines, tabs, carriage
    // returns These can be used for command injection even with metacharacter
    // checks
    for (char c : cmd_str) {
        if (std::iscntrl(static_cast<unsigned char>(c))) {
            if (logger) {
                logger->log(::LLMEngine::LogLevel::Error,
                            "execCommand: Command contains control characters "
                            "(newlines, tabs, etc.) - rejected for security");
            }
            return output;
        }
    }

    // Explicitly check for newlines and tabs (redundant but defensive)
    if (cmd_str.find('\n') != std::string::npos || cmd_str.find('\r') != std::string::npos ||
        cmd_str.find('\t') != std::string::npos) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Error,
                        "execCommand: Command contains newlines, carriage "
                        "returns, or tabs - rejected for security");
        }
        return output;
    }

    // Check if command matches whitelist pattern (no \s, only explicit space)
    if (!std::regex_match(cmd_str, SAFE_CHARS_REGEX)) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Error,
                        std::string("execCommand: Command contains potentially "
                                    "unsafe characters: ") +
                            cmd_str);
            logger->log(::LLMEngine::LogLevel::Error,
                        "Only alphanumeric, single spaces, hyphens, underscores, "
                        "dots, and slashes are allowed");
            logger->log(::LLMEngine::LogLevel::Error,
                        "For security reasons, shell metacharacters and control "
                        "characters are not permitted");
        }
        return output;
    }

    // Additional check: prevent command chaining attempts via shell
    // metacharacters
    if (cmd_str.find('|') != std::string::npos || cmd_str.find('&') != std::string::npos ||
        cmd_str.find(';') != std::string::npos || cmd_str.find('$') != std::string::npos ||
        cmd_str.find('`') != std::string::npos || cmd_str.find('(') != std::string::npos ||
        cmd_str.find(')') != std::string::npos || cmd_str.find('<') != std::string::npos ||
        cmd_str.find('>') != std::string::npos || cmd_str.find('{') != std::string::npos ||
        cmd_str.find('}') != std::string::npos || cmd_str.find('[') != std::string::npos ||
        cmd_str.find(']') != std::string::npos || cmd_str.find('*') != std::string::npos ||
        cmd_str.find('?') != std::string::npos || cmd_str.find('!') != std::string::npos ||
        cmd_str.find('#') != std::string::npos || cmd_str.find('~') != std::string::npos) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Error,
                        "execCommand: Command contains shell metacharacters - "
                        "rejected for security");
        }
        return output;
    }

    // Prevent multiple consecutive spaces (could be used to hide malicious
    // content)
    if (cmd_str.find("  ") != std::string::npos) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Error,
                        "execCommand: Command contains multiple consecutive "
                        "spaces - rejected for security");
        }
        return output;
    }

    // Parse command string into argv array (split on spaces)
    std::vector<std::string> args;
    std::istringstream iss(cmd_str);
    std::string arg;
    while (iss >> arg) {
        args.push_back(arg);
        if (args.size() > MAX_ARG_COUNT) {
            if (logger) {
                logger->log(::LLMEngine::LogLevel::Error,
                            "execCommand: Too many arguments - rejected for "
                            "security");
            }
            return output;
        }
        if (arg.size() > MAX_ARG_LENGTH) {
            if (logger) {
                logger->log(::LLMEngine::LogLevel::Error,
                            "execCommand: Argument too long - rejected for "
                            "security");
            }
            return output;
        }
    }

    // Call the implementation with pre-parsed arguments
    return execCommandImpl(args, logger, cmd_str);
}

std::vector<std::string> execCommand(const std::vector<std::string>& args,
                                     ::LLMEngine::Logger* logger) {
    // Build command string for logging purposes
    std::string cmd_str_for_logging;
    if (!args.empty()) {
        cmd_str_for_logging = args[0];
        for (size_t i = 1; i < args.size(); ++i) {
            cmd_str_for_logging += " " + args[i];
        }
    }
    // Call implementation directly without validation (trusted input)
    return execCommandImpl(args, logger, cmd_str_for_logging);
}

std::string stripMarkdown(std::string_view input) {
    std::string output = std::string(input);
    output = std::regex_replace(output, MARKDOWN_BOLD_REGEX, "");
    output = std::regex_replace(output, MARKDOWN_HEADER_REGEX, "");
    return output;
}

bool validateApiKey(std::string_view api_key) {
    if (api_key.empty()) {
        return false;
    }

    // Check length (reasonable bounds)
    if (api_key.size() < MIN_API_KEY_LENGTH || api_key.size() > MAX_API_KEY_LENGTH) {
        return false;
    }

    // Check for control characters
    if (!std::ranges::all_of(api_key, [](char c) {
            return !std::iscntrl(static_cast<unsigned char>(c));
        })) {
        return false;
    }

    return true;
}

bool validateModelName(std::string_view model_name) {
    if (model_name.empty()) {
        return false;
    }

    // Check length
    if (model_name.size() > MAX_MODEL_NAME_LENGTH) {
        return false;
    }

    // Check for allowed characters: alphanumeric, hyphens, underscores, dots,
    // slashes
    if (!std::ranges::all_of(model_name, [](char c) {
            return std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' ||
                   c == '.' || c == '/';
        })) {
        return false;
    }

    return true;
}

bool validateUrl(std::string_view url) {
    if (url.empty()) {
        return false;
    }

    // Check length
    if (url.size() > MAX_URL_LENGTH) {
        return false;
    }

    // Must start with http:// or https://
    if (url.size() < MIN_URL_LENGTH) {
        return false;
    }

    if (url.substr(0, HTTP_PREFIX_LENGTH) != "http://" &&
        url.substr(0, HTTPS_PREFIX_LENGTH) != "https://") {
        return false;
    }

    // Check for control characters
    if (!std::ranges::all_of(url, [](char c) {
            return !std::iscntrl(static_cast<unsigned char>(c));
        })) {
        return false;
    }

    return true;
}
} // namespace LLMEngine::Utils
