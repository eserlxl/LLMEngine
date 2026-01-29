// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "LLMEngine/utils/Logger.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace LLMEngine::Utils {

/**
 * @brief Execute a shell command securely and return output lines.
 *
 * VALIDATION:
 * - Checks length limits
 * - Checks for control characters
 * - Checks for shell metacharacters
 * - Pipes stdout/stderr and returns them as lines
 */
std::vector<std::string> execCommand(std::string_view cmd, ::LLMEngine::Logger* logger = nullptr);

/**
 * @brief Execute a command with pre-parsed arguments.
 *
 * This version skips most shell injection checks as it passes args directly to execve/posix_spawn.
 */
std::vector<std::string> execCommand(const std::vector<std::string>& args,
                                     ::LLMEngine::Logger* logger = nullptr);

} // namespace LLMEngine::Utils
