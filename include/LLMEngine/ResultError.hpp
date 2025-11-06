// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include "LLMEngine/ErrorCodes.hpp"
#include "LLMEngine/Result.hpp"
#include <string>

namespace LLMEngine {

/**
 * @brief Error type for use with Result<T, E> that includes error code and message.
 * 
 * This type provides structured error information combining the error code
 * with a human-readable error message.
 */
struct LLMENGINE_EXPORT ResultError {
    LLMEngineErrorCode code;
    std::string message;
    
    ResultError(LLMEngineErrorCode code, std::string message)
        : code(code), message(std::move(message)) {}
    
    ResultError(LLMEngineErrorCode code, const char* message)
        : code(code), message(message) {}
};

/**
 * @brief Convenience alias for Result with ResultError.
 * 
 * Use this for functions that can fail with structured error information.
 * 
 * @example
 * ```cpp
 * Result<std::string, ResultError> parseConfig(const std::string& path) {
 *     if (path.empty()) {
 *         return Result<std::string, ResultError>::err(
 *             ResultError(LLMEngineErrorCode::Client, "Path cannot be empty"));
 *     }
 *     // ... parse ...
 *     return Result<std::string, ResultError>::ok(parsed_value);
 * }
 * ```
 */
template<typename T>
using ResultOrError = Result<T, ResultError>;

} // namespace LLMEngine

