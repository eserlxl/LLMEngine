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
    nlohmann::json out;
    const bool changed = mergeInto(base_params, input, mode, out);
    if (!changed) {
        return base_params;
    }
    return out;
}

bool ParameterMerger::mergeInto(
    const nlohmann::json& base_params,
    const nlohmann::json& input,
    std::string_view mode,
    nlohmann::json& out) {
    bool needs_merge = false;
    if (input.contains("max_tokens") && input["max_tokens"].is_number_integer()) {
        const int max_tokens = input["max_tokens"].get<int>();
        if (max_tokens > 0) {
            needs_merge = true;
        }
    }
    if (!mode.empty()) {
        needs_merge = true;
    }

    if (!needs_merge) {
        return false;
    }

    out = base_params;

    if (input.contains("max_tokens") && input["max_tokens"].is_number_integer()) {
        const int max_tokens = input["max_tokens"].get<int>();
        if (max_tokens > 0) {
            out["max_tokens"] = max_tokens;
        }
    }

    if (!mode.empty()) {
        out["mode"] = std::string(mode);
    }

    return true;
}

} // namespace LLMEngine

