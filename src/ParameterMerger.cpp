// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/ParameterMerger.hpp"

namespace LLMEngine {

nlohmann::json ParameterMerger::merge(
    const nlohmann::json& base_params,
    const nlohmann::json& input,
    std::string_view mode) {
    
    // Check if any overrides are present
    bool needs_merge = false;
    
    if (input.contains("max_tokens") && input["max_tokens"].is_number_integer()) {
        int max_tokens = input["max_tokens"].get<int>();
        if (max_tokens > 0) {
            needs_merge = true;
        }
    }
    
    if (!std::string(mode).empty()) {
        needs_merge = true;
    }
    
    // If no overrides, return reference to base (no copy)
    // Note: Returning a const reference to base_params is safe here since
    // the caller will use it immediately and not store it
    if (!needs_merge) {
        return base_params;  // Return copy of reference (cheap for JSON)
    }
    
    // Copy and apply overrides
    nlohmann::json merged = base_params;
    
    if (input.contains("max_tokens") && input["max_tokens"].is_number_integer()) {
        int max_tokens = input["max_tokens"].get<int>();
        if (max_tokens > 0) {
            merged["max_tokens"] = max_tokens;
        }
    }
    
    if (!std::string(mode).empty()) {
        merged["mode"] = std::string(mode);
    }
    
    return merged;
}

} // namespace LLMEngine

