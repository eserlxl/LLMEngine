// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include "LLMEngine/LLMEngineExport.hpp"

#include <nlohmann/json.hpp>
#include <string_view>

namespace LLMEngine {
struct Logger; // Forward declaration

/**
 * @brief Merges model parameters with input overrides.
 *
 * Handles lazy copying to avoid unnecessary allocations when no overrides
 * are present. This service encapsulates parameter merging logic.
 */
class LLMENGINE_EXPORT ParameterMerger {
  public:
    /**
     * @brief Merge base model parameters with input overrides.
     *
     * Returns a value. When no overrides are present, returns a copy of base_params
     * (NRVO typically elides copies). Supports max_tokens and mode overrides from input.
     *
     * @param base_params Base model parameters
     * @param input Input JSON that may contain overrides
     * @param mode Optional mode override
     * @return Merged parameters value
     *
     * @example
     * ```cpp
     * nlohmann::json base = {{"temperature", 0.7}, {"max_tokens", 1000}};
     * nlohmann::json input = {{"temperature", 0.5}};
     * auto merged = ParameterMerger::merge(base, input, "");
     * // merged contains: {"temperature": 0.5, "max_tokens": 1000}
     * ```
     *
     * @example
     * ```cpp
     * nlohmann::json base = {{"temperature", 0.7}};
     * nlohmann::json input = {};
     * auto merged = ParameterMerger::merge(base, input, "chat");
     * // merged contains: {"temperature": 0.7, "mode": "chat"}
     * ```
     */
    [[nodiscard]] static nlohmann::json merge(const nlohmann::json& base_params,
                                              const nlohmann::json& input,
                                              std::string_view mode);

    /**
     * @brief Merge into an output object to avoid copies when unchanged.
     *
     * This method is more efficient than merge() when you want to avoid
     * unnecessary copies. It only modifies 'out' if changes are needed.
     *
     * @param base_params Base parameters (unchanged)
     * @param input Input overrides
     * @param mode Optional mode override
     * @param out Output object receiving merged params when changes are needed
     * @param logger Optional logger for warning messages when type mismatches occur
     * @return true if changes were applied and 'out' was written, false if no changes were needed
     *
     * @example
     * ```cpp
     * nlohmann::json base = {{"temperature", 0.7}};
     * nlohmann::json input = {{"max_tokens", 2000}};
     * nlohmann::json result;
     * if (ParameterMerger::mergeInto(base, input, "", result, logger)) {
     *     // result contains merged parameters
     *     // Use result instead of base
     * } else {
     *     // No changes needed, use base directly
     * }
     * ```
     */
    static bool mergeInto(const nlohmann::json& base_params,
                          const nlohmann::json& input,
                          std::string_view mode,
                          nlohmann::json& out,
                          Logger* logger = nullptr);
};

} // namespace LLMEngine
