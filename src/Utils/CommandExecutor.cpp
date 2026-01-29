// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LLMEngine/Utils/CommandExecutor.hpp"

#include "LLMEngine/Logger.hpp"

// POSIX-specific includes

#include <array>
#include <cerrno>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#if defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__))
#include <spawn.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h> // for environ
#elif defined(_WIN32) || defined(_WIN64)
#include <io.h>
#endif

namespace LLMEngine::Utils {

// Constants extracted from Utils.cpp
constexpr size_t kCommandBufferCount = 256;
constexpr size_t kMaxOutputLines = 10000;
constexpr size_t kMaxLineLength = static_cast<size_t>(1024) * 1024; // 1MB
constexpr size_t kMaxCmdStringLength = 4096;
constexpr size_t kMaxArgCount = 64;
constexpr size_t kMaxArgLength = 512;

// Precompiled regex patterns
static const std::regex kSafeCharsRegex(R"([a-zA-Z0-9_./ -]+)");

namespace {
void closeFd(int fd) {
#if defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__))
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
            closeFd(fd_);
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
                closeFd(fd_);
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

// Internal implementation
std::vector<std::string> execCommandImpl(const std::vector<std::string>& args,
                                         ::LLMEngine::Logger* logger,
                                         const std::string& cmdStrForLogging) {
    std::vector<std::string> output;

#if defined(_WIN32) || defined(_WIN64)
    if (logger) {
        logger->log(::LLMEngine::LogLevel::Error,
                    "execCommand: Not available on Windows. This function "
                    "requires POSIX spawn API.");
    }
    return output;
#else

    if (args.empty()) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Error, "execCommand: Empty command string");
        }
        return output;
    }

    if (cmdStrForLogging.size() > kMaxCmdStringLength) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Error,
                        "execCommand: Command string too long - rejected for security");
        }
        return output;
    }
    if (args.size() > kMaxArgCount) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Error,
                        "execCommand: Too many arguments - rejected for security");
        }
        return output;
    }
    for (const auto& a : args) {
        if (a.size() > kMaxArgLength || !std::regex_match(a, kSafeCharsRegex)) {
            if (logger) {
                logger->log(::LLMEngine::LogLevel::Error,
                            "execCommand: Argument validation failed - rejected for security");
            }
            return output;
        }
    }

    // Check for control characters
    for (char c : cmdStrForLogging) {
        if (std::iscntrl(static_cast<unsigned char>(c))) {
            if (logger) {
                logger->log(::LLMEngine::LogLevel::Error,
                            "execCommand: Command contains control characters (newlines, tabs, etc.) "
                            "- rejected for security");
            }
            return output;
        }
    }

    if (cmdStrForLogging.find('\n') != std::string::npos
        || cmdStrForLogging.find('\r') != std::string::npos
        || cmdStrForLogging.find('\t') != std::string::npos) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Error,
                        "execCommand: Command contains newlines, carriage returns, or tabs - "
                        "rejected for security");
        }
        return output;
    }

    if (!std::regex_match(cmdStrForLogging, kSafeCharsRegex)) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Error,
                        std::string("execCommand: Command contains potentially unsafe characters: ")
                            + cmdStrForLogging);
        }
        return output;
    }

    if (cmdStrForLogging.find('|') != std::string::npos
        || cmdStrForLogging.find('&') != std::string::npos
        || cmdStrForLogging.find(';') != std::string::npos
        || cmdStrForLogging.find('$') != std::string::npos
        || cmdStrForLogging.find('`') != std::string::npos
        || cmdStrForLogging.find('(') != std::string::npos
        || cmdStrForLogging.find(')') != std::string::npos
        || cmdStrForLogging.find('<') != std::string::npos
        || cmdStrForLogging.find('>') != std::string::npos
        || cmdStrForLogging.find('{') != std::string::npos
        || cmdStrForLogging.find('}') != std::string::npos
        || cmdStrForLogging.find('[') != std::string::npos
        || cmdStrForLogging.find(']') != std::string::npos
        || cmdStrForLogging.find('*') != std::string::npos
        || cmdStrForLogging.find('?') != std::string::npos
        || cmdStrForLogging.find('!') != std::string::npos
        || cmdStrForLogging.find('#') != std::string::npos
        || cmdStrForLogging.find('~') != std::string::npos) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Error,
                        "execCommand: Command contains shell metacharacters - rejected for security");
        }
        return output;
    }

    if (cmdStrForLogging.find("  ") != std::string::npos) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Error,
                        "execCommand: Command contains multiple consecutive spaces - rejected for "
                        "security");
        }
        return output;
    }

    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (auto& argStr : args) {
        argv.push_back(const_cast<char*>(argStr.c_str()));
    }
    argv.push_back(nullptr);

    std::array<int, 2> stdoutPipe;
    std::array<int, 2> stderrPipe;
    if (pipe(stdoutPipe.data()) != 0 || pipe(stderrPipe.data()) != 0) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Error,
                        "execCommand: Failed to create pipes for command");
        }
        return output;
    }

    PipeFD stdoutRead(stdoutPipe[0]);
    PipeFD stdoutWrite(stdoutPipe[1]);
    PipeFD stderrRead(stderrPipe[0]);
    PipeFD stderrWrite(stderrPipe[1]);

    posix_spawn_file_actions_t fileActions;
    if (posix_spawn_file_actions_init(&fileActions) != 0) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Error,
                        "execCommand: posix_spawn_file_actions_init failed");
        }
        return output;
    }

    auto addAction = [&](int rc, const char* what) -> bool {
        if (rc != 0) {
            if (logger) {
                logger->log(::LLMEngine::LogLevel::Error,
                            std::string("execCommand: ") + what + " failed (" + std::to_string(rc)
                                + ")");
            }
            posix_spawn_file_actions_destroy(&fileActions);
            return false;
        }
        return true;
    };

    if (!addAction(posix_spawn_file_actions_addclose(&fileActions, stdoutRead.get()),
                    "addclose stdoutRead"))
        return output;
    if (!addAction(posix_spawn_file_actions_addclose(&fileActions, stderrRead.get()),
                    "addclose stderrRead"))
        return output;
    if (!addAction(
            posix_spawn_file_actions_adddup2(&fileActions, stdoutWrite.get(), STDOUT_FILENO),
            "adddup2 stdout"))
        return output;
    if (!addAction(
            posix_spawn_file_actions_adddup2(&fileActions, stderrWrite.get(), STDERR_FILENO),
            "adddup2 stderr"))
        return output;
    if (!addAction(posix_spawn_file_actions_addclose(&fileActions, stdoutWrite.get()),
                    "addclose stdoutWrite"))
        return output;
    if (!addAction(posix_spawn_file_actions_addclose(&fileActions, stderrWrite.get()),
                    "addclose stderrWrite"))
        return output;

    pid_t pid;
    int spawnResult = posix_spawnp(&pid, argv[0], &fileActions, nullptr, argv.data(), environ);

    posix_spawn_file_actions_destroy(&fileActions);

    if (spawnResult != 0) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Error,
                        std::string("execCommand: posix_spawnp() failed (error: ")
                            + std::to_string(spawnResult) + ")");
        }
        return output;
    }

    stdoutWrite = PipeFD(-1);
    stderrWrite = PipeFD(-1);

    std::array<char, kCommandBufferCount> buffer;
    std::string stdoutBuffer;
    std::string stderrBuffer;
    size_t stdoutTotalSize = 0;
    size_t stderrTotalSize = 0;

    std::array<struct pollfd, 2> pollfds;
    pollfds[0].fd = stdoutRead.get();
    pollfds[0].events = POLLIN;
    pollfds[1].fd = stderrRead.get();
    pollfds[1].events = POLLIN;

    bool stdoutClosed = false;
    bool stderrClosed = false;

    while (!stdoutClosed || !stderrClosed) {
        std::vector<struct pollfd> activePollFds;
        std::vector<int> fdToIndex;

        if (!stdoutClosed) {
            activePollFds.push_back(pollfds[0]);
            fdToIndex.push_back(0);
        }
        if (!stderrClosed) {
            activePollFds.push_back(pollfds[1]);
            fdToIndex.push_back(1);
        }

        if (activePollFds.empty())
            break;

        int pollResult = poll(activePollFds.data(), activePollFds.size(), -1);
        if (pollResult < 0) {
            if (logger) {
                logger->log(::LLMEngine::LogLevel::Warn,
                            "execCommand: poll() failed, terminating read");
            }
            break;
        }

        for (size_t i = 0; i < activePollFds.size(); ++i) {
            pollfds[static_cast<size_t>(fdToIndex[i])].revents = activePollFds[i].revents;
        }

        if (!stdoutClosed && (pollfds[0].revents & POLLIN)) {
            ssize_t bytesRead = read(stdoutRead.get(), buffer.data(), buffer.size());
            if (bytesRead > 0) {
                stdoutTotalSize += static_cast<size_t>(bytesRead);
                if (stdoutTotalSize > kMaxLineLength * kMaxOutputLines) {
                    if (logger) {
                        logger->log(::LLMEngine::LogLevel::Warn,
                                    "execCommand: Output exceeds maximum size limit, truncating");
                    }
                    stdoutClosed = true;
                } else {
                    stdoutBuffer.append(buffer.data(), static_cast<size_t>(bytesRead));
                }
            } else if (bytesRead == 0
                       || (bytesRead < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                stdoutClosed = true;
                pollfds[0].fd = -1;
            }
        } else if (!stdoutClosed && (pollfds[0].revents & (POLLHUP | POLLERR))) {
            stdoutClosed = true;
            pollfds[0].fd = -1;
        }

        if (!stderrClosed && (pollfds[1].revents & POLLIN)) {
            ssize_t bytesRead = read(stderrRead.get(), buffer.data(), buffer.size());
            if (bytesRead > 0) {
                stderrTotalSize += static_cast<size_t>(bytesRead);
                if (stderrTotalSize > kMaxLineLength * kMaxOutputLines) {
                    if (logger) {
                        logger->log(::LLMEngine::LogLevel::Warn,
                                    "execCommand: Error output exceeds maximum size limit, truncating");
                    }
                    stderrClosed = true;
                } else {
                    stderrBuffer.append(buffer.data(), static_cast<size_t>(bytesRead));
                }
            } else if (bytesRead == 0
                       || (bytesRead < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                stderrClosed = true;
                pollfds[1].fd = -1;
            }
        } else if (!stderrClosed && (pollfds[1].revents & (POLLHUP | POLLERR))) {
            stderrClosed = true;
            pollfds[1].fd = -1;
        }
    }

    size_t totalLines = 0;
    std::istringstream stdoutStream(stdoutBuffer);
    std::string line;
    while (std::getline(stdoutStream, line) && totalLines < kMaxOutputLines) {
        if (line.size() > kMaxLineLength) {
            line.resize(kMaxLineLength);
        }
        output.push_back(line);
        totalLines++;
    }

    std::istringstream stderrStream(stderrBuffer);
    while (std::getline(stderrStream, line) && totalLines < kMaxOutputLines) {
        if (line.size() > kMaxLineLength) {
            line.resize(kMaxLineLength);
        }
        output.push_back(line);
        totalLines++;
    }

    int status;
    if (waitpid(pid, &status, 0) == -1) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Error,
                        "execCommand: waitpid() failed for command");
        }
        return output;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Warn,
                        std::string("Command '") + cmdStrForLogging
                            + "' exited with non-zero status: "
                            + std::to_string(WEXITSTATUS(status)));
        }
    } else if (WIFSIGNALED(status)) {
        if (logger) {
            logger->log(::LLMEngine::LogLevel::Warn,
                        std::string("Command '") + cmdStrForLogging
                            + "' terminated by signal: " + std::to_string(WTERMSIG(status)));
        }
    }

    return output;
#endif // WIN32
}

std::vector<std::string> execCommand(std::string_view cmd, ::LLMEngine::Logger* logger) {
    std::vector<std::string> output;

#if defined(_WIN32) || defined(_WIN64)
    if (logger) {
        logger->log(::LLMEngine::LogLevel::Error, "execCommand: Not supported on Windows");
    }
    return output;
#else
    std::string cmdStr(cmd);
    if (cmdStr.empty()) return output;

    if (cmdStr.size() > kMaxCmdStringLength) {
        if (logger) logger->log(::LLMEngine::LogLevel::Error, "execCommand: Command too long");
        return output;
    }

    std::vector<std::string> args;
    std::istringstream iss(cmdStr);
    std::string arg;
    while (iss >> arg) {
        args.push_back(arg);
    }
    
    return execCommandImpl(args, logger, cmdStr);
#endif
}

std::vector<std::string> execCommand(const std::vector<std::string>& args,
                                     ::LLMEngine::Logger* logger) {
    std::string cmdStrForLogging;
    if (!args.empty()) {
        cmdStrForLogging = args[0];
        for (size_t i = 1; i < args.size(); ++i) {
            cmdStrForLogging += " " + args[i];
        }
    }
    return execCommandImpl(args, logger, cmdStrForLogging);
}

} // namespace LLMEngine::Utils
