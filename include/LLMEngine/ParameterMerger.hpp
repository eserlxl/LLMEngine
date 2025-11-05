// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include <nlohmann/json.hpp>
#include <string_view>
#include "LLMEngine/LLMEngineExport.hpp"

namespace LLMEngine {

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
     */
    [[nodiscard]] static nlohmann::json merge(
        const nlohmann::json& base_params,
        const nlohmann::json& input,
        std::string_view mode);

    /**
     * @brief Merge into an output object to avoid copies when unchanged.
     * @param base_params Base parameters (unchanged)
     * @param input Input overrides
     * @param mode Optional mode override
     * @param out Output object receiving merged params when changes are needed
     * @return true if changes were applied and 'out' was written, false if no changes were needed
     */
    static bool mergeInto(
        const nlohmann::json& base_params,
        const nlohmann::json& input,
        std::string_view mode,
        nlohmann::json& out);
};

} // namespace LLMEngine

