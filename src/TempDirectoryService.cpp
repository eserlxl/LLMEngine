// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/TempDirectoryService.hpp"
#include "LLMEngine/Logger.hpp"
#include <filesystem>
#include <system_error>

namespace LLMEngine {

TempDirectoryService::DirectoryResult TempDirectoryService::ensureSecureDirectory(
    const std::string& directory_path, Logger* logger) {

    DirectoryResult result;
    result.success = false;

    // Security: Check for symlink before creating directories
    // If the path exists and is a symlink, reject it to prevent symlink traversal attacks
    std::error_code ec;
    if (std::filesystem::exists(directory_path, ec)) {
        std::error_code ec_symlink;
        if (std::filesystem::is_symlink(directory_path, ec_symlink)) {
            result.error_message =
                "Temporary directory cannot be a symlink for security reasons: " + directory_path;
            if (logger) {
                logger->log(LogLevel::Error, result.error_message);
            }
            return result;
        }
    }

    // Create directory
    std::filesystem::create_directories(directory_path, ec);
    if (ec) {
        result.error_message =
            "Failed to create temporary directory: " + directory_path + ": " + ec.message();
        if (logger) {
            logger->log(LogLevel::Error, result.error_message);
        }
        return result;
    }

    // Set permissions to 0700 (owner-only access)
    if (std::filesystem::exists(directory_path)) {
        std::error_code ec_perm;
        std::filesystem::permissions(directory_path,
                                     std::filesystem::perms::owner_read |
                                         std::filesystem::perms::owner_write |
                                         std::filesystem::perms::owner_exec,
                                     std::filesystem::perm_options::replace, ec_perm);
        if (ec_perm && logger) {
            logger->log(LogLevel::Warn, std::string("Failed to set permissions on ") +
                                            directory_path + ": " + ec_perm.message());
        }
    }

    result.success = true;
    return result;
}

bool TempDirectoryService::validatePathWithinRoot(const std::string& requested_path,
                                                  const std::string& allowed_root, Logger* logger) {

    try {
        const std::filesystem::path default_root =
            std::filesystem::path(allowed_root).lexically_normal();
        const std::filesystem::path requested =
            std::filesystem::path(requested_path).lexically_normal();

        // Ensure requested is under default_root using a robust relative check
        const std::filesystem::path rel =
            std::filesystem::weakly_canonical(requested).lexically_relative(
                std::filesystem::weakly_canonical(default_root));

        bool has_parent_ref = false;
        for (const auto& part : rel) {
            if (part == "..") {
                has_parent_ref = true;
                break;
            }
        }

        const bool is_within = !rel.empty() && !rel.is_absolute() && !has_parent_ref;

        if (!is_within && logger) {
            logger->log(
                LogLevel::Warn,
                std::string("Rejected temp directory outside allowed root: ") + requested.string());
        }

        return is_within;
    } catch (...) {
        if (logger) {
            logger->log(LogLevel::Error,
                        std::string("Failed to validate temp directory due to path error: ") +
                            requested_path);
        }
        return false;
    }
}

bool TempDirectoryService::isDirectoryValid(const std::string& directory_path, Logger* logger) {
    std::error_code ec;
    if (!std::filesystem::exists(directory_path, ec)) {
        return false;
    }

    std::error_code ec_symlink;
    if (std::filesystem::is_symlink(directory_path, ec_symlink)) {
        if (logger) {
            logger->log(LogLevel::Error,
                        std::string("Temporary directory is a symlink: ") + directory_path);
        }
        return false;
    }

    return true;
}

} // namespace LLMEngine
