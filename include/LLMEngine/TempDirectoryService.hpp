// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include <string>
#include "LLMEngine/LLMEngineExport.hpp"

// Forward declaration
namespace LLMEngine {
    struct Logger;
}

namespace LLMEngine {

/**
 * @brief Service for managing temporary directories with security checks.
 * 
 * This class extracts filesystem security and directory management logic from
 * LLMEngine, making it testable in isolation and reusable.
 * 
 * ## Responsibilities
 * 
 * - Directory creation with security checks
 * - Symlink protection (prevents symlink traversal attacks)
 * - Permission setting (0700 owner-only)
 * - Path validation (ensures paths are within allowed root)
 * - Directory existence verification
 * 
 * ## Thread Safety
 * 
 * This class is stateless and thread-safe. All methods are const.
 * 
 * ## Security Features
 * 
 * - **Symlink Protection**: Rejects symlinks to prevent traversal attacks
 * - **Permission Enforcement**: Sets 0700 (owner-only) permissions
 * - **Path Validation**: Ensures requested paths are within allowed root
 * 
 * ## Example Usage
 * 
 * ```cpp
 * TempDirectoryService service;
 * 
 * // Ensure directory exists with security checks
 * if (!service.ensureSecureDirectory("/tmp/llmengine", logger.get())) {
 *     throw std::runtime_error("Failed to create temp directory");
 * }
 * 
 * // Validate and set a new path
 * if (service.validateAndSetPath("/tmp/llmengine/custom", "/tmp/llmengine", logger.get())) {
 *     // Path is valid and set
 * }
 * ```
 */
class LLMENGINE_EXPORT TempDirectoryService {
public:
    /**
     * @brief Result of directory operations.
     */
    struct DirectoryResult {
        bool success;
        std::string error_message;
    };
    
    /**
     * @brief Ensure a directory exists with security checks.
     * 
     * Creates the directory if it doesn't exist, with the following security checks:
     * - Rejects symlinks (prevents symlink traversal attacks)
     * - Sets 0700 permissions (owner-only access)
     * - Handles errors gracefully
     * 
     * @param directory_path Path to the directory to ensure.
     * @param logger Optional logger for error messages.
     * @return DirectoryResult indicating success or failure.
     */
    [[nodiscard]] DirectoryResult ensureSecureDirectory(
        const std::string& directory_path,
        Logger* logger = nullptr
    ) const;
    
    /**
     * @brief Validate that a requested path is within an allowed root.
     * 
     * Checks that the requested path is a subdirectory of the allowed root,
     * preventing directory traversal attacks.
     * 
     * @param requested_path Path to validate.
     * @param allowed_root Root directory that must contain the requested path.
     * @param logger Optional logger for warnings.
     * @return true if path is valid and within allowed root, false otherwise.
     */
    [[nodiscard]] bool validatePathWithinRoot(
        const std::string& requested_path,
        const std::string& allowed_root,
        Logger* logger = nullptr
    ) const;
    
    /**
     * @brief Check if a directory exists and is not a symlink.
     * 
     * @param directory_path Path to check.
     * @param logger Optional logger for errors.
     * @return true if directory exists and is not a symlink, false otherwise.
     */
    [[nodiscard]] bool isDirectoryValid(const std::string& directory_path, Logger* logger = nullptr) const;
};

} // namespace LLMEngine

