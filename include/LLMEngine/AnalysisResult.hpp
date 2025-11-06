// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include <string>
#include "LLMEngine/LLMEngineExport.hpp"
#include "LLMEngine/ErrorCodes.hpp"

namespace LLMEngine {

// Backward compatibility alias - use LLMEngineErrorCode in new code
using AnalysisErrorCode = LLMEngineErrorCode;

/**
 * @brief Result structure returned by LLMEngine::analyze().
 * 
 * Contains the success status, extracted content, and error information.
 */
struct LLMENGINE_EXPORT AnalysisResult {
    bool success;
    std::string think;
    std::string content;
    std::string errorMessage;
    int statusCode;
    
    /**
     * @brief Structured error code for programmatic error handling.
     * 
     * Use this instead of parsing errorMessage for error classification.
     * Only valid when success == false.
     */
    LLMEngineErrorCode errorCode = LLMEngineErrorCode::None;
    
    /**
     * @brief Check if the result represents a specific error type.
     */
    [[nodiscard]] bool hasError(AnalysisErrorCode code) const {
        return !success && errorCode == code;
    }
    
    /**
     * @brief Check if the result is a retriable error (network, timeout, server, rate limit).
     */
    [[nodiscard]] bool isRetriableError() const {
        return !success && (errorCode == LLMEngineErrorCode::Network ||
                           errorCode == LLMEngineErrorCode::Timeout ||
                           errorCode == LLMEngineErrorCode::Server ||
                           errorCode == LLMEngineErrorCode::RateLimited);
    }
};

} // namespace LLMEngine

