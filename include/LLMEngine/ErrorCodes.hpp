// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include "LLMEngine/LLMEngineExport.hpp"

namespace LLMEngine {

/**
 * @brief Unified error code enumeration for LLMEngine.
 *
 * This enum replaces the previous separate AnalysisErrorCode and APIResponse::APIError
 * enums to provide a single source of truth for error classification across the library.
 *
 * ## Thread Safety
 *
 * This enum is thread-safe (enum values are immutable).
 */
enum class LLMENGINE_EXPORT LLMEngineErrorCode {
    None,            ///< No error (success)
    Network,         ///< Network connection error
    Timeout,         ///< Request timeout
    InvalidResponse, ///< Invalid or unparseable response
    Auth,            ///< Authentication/authorization error
    RateLimited,     ///< Rate limit exceeded
    Server,          ///< Server error (5xx)
    Client,          ///< Client error (4xx, non-auth)
    Unknown          ///< Unknown or unclassified error
};

} // namespace LLMEngine
