// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include "LLMEngine/LLMEngineExport.hpp"
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

// Forward declarations
namespace LLMEngine {
struct Logger;
enum class LogLevel;
} // namespace LLMEngine

namespace LLMEngineAPI {
struct APIResponse;
}

namespace LLMEngine {

/**
 * @brief Manages debug artifact creation and cleanup for analysis requests.
 *
 * Encapsulates all debug file writing and cleanup logic, providing a clean
 * interface for the analysis pipeline.
 *
 * ## Thread Safety
 *
 * **DebugArtifactManager is thread-safe** for per-instance usage. Each
 * request should use its own instance or ensure proper synchronization.
 */
class LLMENGINE_EXPORT DebugArtifactManager {
public:
    /**
     * @brief Construct a debug artifact manager.
     *
     * @param request_tmp_dir Unique temporary directory for this request
     * @param base_tmp_dir Base temporary directory for cleanup operations
     * @param log_retention_hours Hours to retain old debug artifacts
     * @param logger Optional logger for debug messages
     */
    DebugArtifactManager(std::string request_tmp_dir, std::string base_tmp_dir,
                         int log_retention_hours, Logger* logger = nullptr);

    /**
     * @brief Write API response JSON (success or error).
     *
     * @param response API response to write
     * @param is_error If true, writes to api_response_error.json, else api_response.json
     * @return true if written successfully
     */
    bool writeApiResponse(const ::LLMEngineAPI::APIResponse& response, bool is_error = false);

    /**
     * @brief Write full response text.
     *
     * @param full_response Complete response text
     * @return true if written successfully
     */
    bool writeFullResponse(std::string_view full_response);

    /**
     * @brief Write analysis-specific artifacts (think section and content).
     *
     * @param analysis_type Analysis type identifier (used for filename)
     * @param think_section Extracted thinking section
     * @param remaining_section Remaining content after thinking extraction
     * @return true if both files written successfully
     */
    bool writeAnalysisArtifacts(std::string_view analysis_type, std::string_view think_section,
                                std::string_view remaining_section);

    /**
     * @brief Get the request temporary directory path.
     *
     * @return Request temporary directory path
     */
    [[nodiscard]] std::string getRequestTmpDir() const {
        return request_tmp_dir_;
    }

    /**
     * @brief Ensure the request directory exists.
     *
     * Creates the directory if it doesn't exist. Safe to call multiple times.
     *
     * @return true if directory exists or was created successfully
     */
    bool ensureRequestDirectory();

    /**
     * @brief Perform cleanup of old debug artifacts.
     *
     * Removes debug artifacts older than the configured retention period.
     * This is called automatically after writing API responses.
     */
    void performCleanup();

private:
    std::string request_tmp_dir_;
    std::string base_tmp_dir_;
    int log_retention_hours_;
    Logger* logger_;
    bool directory_created_;
    mutable std::chrono::system_clock::time_point last_cleanup_time_;
    mutable bool cleanup_time_initialized_;
};

} // namespace LLMEngine
