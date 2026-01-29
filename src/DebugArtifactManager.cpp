// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/DebugArtifactManager.hpp"

#include "DebugArtifacts.hpp"
#include "LLMEngine/APIClient.hpp"
#include "LLMEngine/Constants.hpp"
#include "LLMEngine/Logger.hpp"

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>

namespace LLMEngine {

DebugArtifactManager::DebugArtifactManager(std::string request_tmp_dir,
                                           std::string base_tmp_dir,
                                           int log_retention_hours,
                                           Logger* logger)
    : request_tmp_dir_(std::move(request_tmp_dir)), base_tmp_dir_(std::move(base_tmp_dir)),
      log_retention_hours_(log_retention_hours), logger_(logger), directory_created_(false),
      cleanup_time_initialized_(false) {}

bool DebugArtifactManager::ensureRequestDirectory() {
    if (directory_created_) {
        return true;
    }

    try {
        std::error_code ec;
        std::filesystem::create_directories(request_tmp_dir_, ec);
        if (ec) {
            if (logger_) {
                logger_->log(LogLevel::Warn,
                             "Failed to create request directory: " + request_tmp_dir_);
            }
            return false;
        }
        directory_created_ = true;
        return true;
    } catch (...) {
        if (logger_) {
            logger_->log(LogLevel::Error,
                         "Exception creating request directory: " + request_tmp_dir_);
        }
        return false;
    }
}

bool DebugArtifactManager::writeApiResponse(const ::LLMEngineAPI::APIResponse& response,
                                            bool is_error) {
    if (!ensureRequestDirectory()) {
        return false;
    }

    std::string filename = is_error
                               ? std::string(Constants::DebugArtifacts::API_RESPONSE_ERROR_JSON)
                               : std::string(Constants::DebugArtifacts::API_RESPONSE_JSON);

    std::string filepath = request_tmp_dir_ + "/" + filename;
    bool success =
        DebugArtifacts::writeJson(filepath, response.rawResponse, /*redactSecrets*/ true);

    if (!success && logger_) {
        logger_->log(LogLevel::Warn, "Failed to write debug artifact: " + filepath);
    } else if (success && logger_) {
        logger_->log(LogLevel::Debug, "API response saved to " + filepath);
    }

    return success;
}

bool DebugArtifactManager::writeFullResponse(std::string_view full_response) {
    if (!ensureRequestDirectory()) {
        return false;
    }

    std::string filepath =
        request_tmp_dir_ + "/" + std::string(Constants::DebugArtifacts::RESPONSE_FULL_TXT);
    bool success = DebugArtifacts::writeText(filepath, full_response, /*redactSecrets*/ true);

    if (!success && logger_) {
        logger_->log(LogLevel::Warn, "Failed to write debug artifact: " + filepath);
    } else if (success && logger_) {
        logger_->log(LogLevel::Debug, "Full response saved to " + filepath);
    }

    performCleanup();
    return success;
}

bool DebugArtifactManager::writeAnalysisArtifacts(std::string_view analysis_type,
                                                  std::string_view think_section,
                                                  std::string_view remaining_section) {
    try {
        if (!ensureRequestDirectory()) {
            return false;
        }

        // Sanitize analysis type for filename (no traversal, no leading dots)
        // Handle UTF-8 by truncating to byte length, rejecting non-ASCII if needed
        auto sanitize_name = [](std::string name) {
            if (name.empty())
                name = "analysis";
            // Remove any path separators outright and sanitize characters
            for (char& ch : name) {
                // Only allow ASCII alphanumeric and safe punctuation
                auto uch = static_cast<unsigned char>(ch);
                if (!(std::isalnum(uch) || ch == '-' || ch == '_' || ch == '.')) {
                    ch = '_';
                }
            }
            // Strip leading dots to avoid hidden files and parent traversal tokens like ".."
            while (!name.empty() && name.front() == '.') {
                name.erase(name.begin());
            }
            if (name.empty())
                name = "analysis";
            constexpr size_t MAX_FILENAME_LENGTH = 64;
            // Truncate by byte length to handle UTF-8 correctly
            // This ensures we stay within the 64-byte limit even with multi-byte characters
            if (name.size() > MAX_FILENAME_LENGTH) {
                name.resize(MAX_FILENAME_LENGTH);
            }
            return name;
        };

        const std::string safe_analysis_name = sanitize_name(std::string(analysis_type));

        // Write think section
        std::string think_filepath = request_tmp_dir_ + "/" + safe_analysis_name
                                     + std::string(Constants::DebugArtifacts::THINK_TXT_SUFFIX);
        bool think_success =
            DebugArtifacts::writeText(think_filepath, think_section, /*redactSecrets*/ true);

        if (!think_success && logger_) {
            logger_->log(LogLevel::Warn, "Failed to write debug artifact: " + think_filepath);
        } else if (think_success && logger_) {
            logger_->log(LogLevel::Debug, "Wrote think section");
        }

        // Write remaining section
        std::string content_filepath = request_tmp_dir_ + "/" + safe_analysis_name
                                       + std::string(Constants::DebugArtifacts::CONTENT_TXT_SUFFIX);
        bool content_success =
            DebugArtifacts::writeText(content_filepath, remaining_section, /*redactSecrets*/ true);

        if (!content_success && logger_) {
            logger_->log(LogLevel::Warn, "Failed to write debug artifact: " + content_filepath);
        } else if (content_success && logger_) {
            logger_->log(LogLevel::Debug, "Wrote remaining section");
        }

        return think_success && content_success;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->log(LogLevel::Error,
                         std::string("Exception in writeAnalysisArtifacts: ") + e.what());
        }
        return false;
    } catch (...) {
        if (logger_) {
            logger_->log(LogLevel::Error, "Unknown exception in writeAnalysisArtifacts");
        }
        return false;
    }
}

void DebugArtifactManager::performCleanup() {
    if (log_retention_hours_ <= 0) {
        return;
    }

    // Throttle cleanup: only run if retention window has elapsed since last cleanup
    const auto now = std::chrono::system_clock::now();
    const auto retention_window = std::chrono::hours(log_retention_hours_);

    if (cleanup_time_initialized_) {
        // Only cleanup if retention window has elapsed
        if ((now - last_cleanup_time_) < retention_window) {
            return; // Skip cleanup - too soon since last run
        }
    }

    // Perform cleanup and update timestamp
    DebugArtifacts::cleanupOld(base_tmp_dir_, log_retention_hours_);
    last_cleanup_time_ = now;
    cleanup_time_initialized_ = true;
}

} // namespace LLMEngine
