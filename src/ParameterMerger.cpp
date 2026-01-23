// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/ParameterMerger.hpp"

#include "LLMEngine/Logger.hpp"

namespace LLMEngine {

nlohmann::json ParameterMerger::merge(const nlohmann::json& base_params,
                                      const nlohmann::json& input,
                                      std::string_view mode) {
    nlohmann::json out;
    const bool changed = mergeInto(base_params, input, mode, out, nullptr);
    if (!changed) {
        return base_params;
    }
    return out;
}

bool ParameterMerger::mergeInto(const nlohmann::json& base_params,
                                const nlohmann::json& input,
                                std::string_view mode,
                                nlohmann::json& out,
                                Logger* logger) {
    // Allow-list of overridable keys and expected types
    static const std::vector<std::pair<const char*, nlohmann::json::value_t>> allowed_keys = {
        {"max_tokens", nlohmann::json::value_t::number_integer},
        {"temperature", nlohmann::json::value_t::number_float},
        {"top_p", nlohmann::json::value_t::number_float},
        {"top_k", nlohmann::json::value_t::number_integer},
        {"min_p", nlohmann::json::value_t::number_float},
        {"presence_penalty", nlohmann::json::value_t::number_float},
        {"frequency_penalty", nlohmann::json::value_t::number_float},
        {"timeout_seconds", nlohmann::json::value_t::number_integer}};

    bool changed = false;
    // Fast check: mode or any allowed key present
    if (!mode.empty())
        changed = true;
    if (!changed) {
        for (const auto& [key, _] : allowed_keys) {
            auto it = input.find(key);
            if (it != input.end()) {
                changed = true;
                break;
            }
        }
    }
    if (!changed)
        return false;

    out = base_params;

    for (const auto& [key, expected] : allowed_keys) {
        auto it = input.find(key);
        if (it == input.end())
            continue;
        const nlohmann::json& val = *it;
        // Accept integer where float expected if convertible
        if (expected == nlohmann::json::value_t::number_float
            && (val.is_number_float() || val.is_number_integer())) {
            out[key] = val.get<double>();
            continue;
        }
        if (val.type() == expected) {
            out[key] = val;
        } else {
            // Type mismatch: log warning to help users understand why override was dropped
            if (logger) {
                std::string expected_type_str;
                switch (expected) {
                    case nlohmann::json::value_t::number_integer:
                        expected_type_str = "integer";
                        break;
                    case nlohmann::json::value_t::number_float:
                        expected_type_str = "float";
                        break;
                    default:
                        expected_type_str = "unknown";
                        break;
                }
                logger->log(LogLevel::Warn,
                            "Parameter override '" + std::string(key)
                                + "' has incorrect type (expected " + expected_type_str
                                + "), ignoring override");
            }
        }
    }

    if (!mode.empty()) {
        out["mode"] = std::string(mode);
    }

    return true;
}

} // namespace LLMEngine
