// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/ParameterMerger.hpp"

namespace LLMEngine {

const nlohmann::json& ParameterMerger::merge(
    const nlohmann::json& base_params,
    const nlohmann::json& input,
    std::string_view mode) {
    
    // Check if any overrides are present (avoid materializing strings)
    bool needs_merge = false;
    
    if (input.contains("max_tokens") && input["max_tokens"].is_number_integer()) {
        int max_tokens = input["max_tokens"].get<int>();
        if (max_tokens > 0) {
            needs_merge = true;
        }
    }
    
    if (!mode.empty()) {
        needs_merge = true;
    }
    
    // If no overrides, return const reference to base_params to avoid allocations
    if (!needs_merge) {
        return base_params;
    }
    
    // Copy and apply overrides
    // Use thread_local static storage to hold merged result (safe for return by reference)
    // This avoids heap allocation while allowing reference return
    thread_local static nlohmann::json merged_storage;
    merged_storage = base_params;
    
    if (input.contains("max_tokens") && input["max_tokens"].is_number_integer()) {
        int max_tokens = input["max_tokens"].get<int>();
        if (max_tokens > 0) {
            merged_storage["max_tokens"] = max_tokens;
        }
    }
    
    if (!mode.empty()) {
        merged_storage["mode"] = std::string(mode);
    }
    
    return merged_storage;
}

} // namespace LLMEngine

