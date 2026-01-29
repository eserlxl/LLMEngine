// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/core/ParameterMerger.hpp"
#include "LLMEngine/core/ParameterMerger.hpp"
#include "LLMEngine/utils/Logger.hpp"
#include "LLMEngine/http/RequestOptions.hpp"


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
        {"timeout_seconds", nlohmann::json::value_t::number_integer},
        {"seed", nlohmann::json::value_t::number_integer},
        {"user", nlohmann::json::value_t::string},
        {"stop", nlohmann::json::value_t::array},
        {"response_format", nlohmann::json::value_t::object},
        {"logit_bias", nlohmann::json::value_t::object},
        {"logprobs", nlohmann::json::value_t::boolean},
        {"top_logprobs", nlohmann::json::value_t::number_integer},
        {"parallel_tool_calls", nlohmann::json::value_t::boolean},
        {"service_tier", nlohmann::json::value_t::string},
        {"reasoning_effort", nlohmann::json::value_t::string},
        {"max_completion_tokens", nlohmann::json::value_t::number_integer}};


    // Verify input is an object
    // Allow null (treated as empty) or object. Reject primitives/arrays.
    if (!input.is_object() && !input.is_null()) {
        if (logger) {
            logger->log(LogLevel::Warn,
                        "ParameterMerger: input is not a JSON object, ignoring overrides");
        }
        return false;
    }

    bool changed = false;
    // Fast check: mode or any allowed key present
    if (!mode.empty()) {
        changed = true;
    }

    // DEBUG only
    // std::cout << "DEBUG: mergeInto mode='" << mode << "' changed=" << changed << std::endl;
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

void ParameterMerger::mergeRequestOptions(nlohmann::json& params, const RequestOptions& options) {
    auto& gen = options.generation;
    if (gen.temperature) params["temperature"] = *gen.temperature;
    if (gen.max_tokens) params["max_tokens"] = *gen.max_tokens;
    if (gen.top_p) params["top_p"] = *gen.top_p;
    if (gen.frequency_penalty) params["frequency_penalty"] = *gen.frequency_penalty;
    if (gen.presence_penalty) params["presence_penalty"] = *gen.presence_penalty;
    if (gen.seed) params["seed"] = *gen.seed;
    if (gen.user) params["user"] = *gen.user;
    if (gen.parallel_tool_calls) params["parallel_tool_calls"] = *gen.parallel_tool_calls;
    if (gen.service_tier) params["service_tier"] = *gen.service_tier;
    if (gen.reasoning_effort) params["reasoning_effort"] = *gen.reasoning_effort;
    if (gen.max_completion_tokens) params["max_completion_tokens"] = *gen.max_completion_tokens;
    if (gen.response_format) params["response_format"] = *gen.response_format;
    if (gen.tool_choice) params["tool_choice"] = *gen.tool_choice;
    
    // Vectors/Complex types
    if (!gen.stop_sequences.empty()) {
        params["stop"] = gen.stop_sequences;
    }
    if (gen.logit_bias) {
        params["logit_bias"] = *gen.logit_bias;
    }
    
    if (gen.logprobs) {
        params["logprobs"] = *gen.logprobs;
        if (gen.top_logprobs) {
            params["top_logprobs"] = *gen.top_logprobs;
        }
    }
    
    if (gen.top_k) params["top_k"] = *gen.top_k;
    if (gen.min_p) params["min_p"] = *gen.min_p;

    if (options.stream_options) {
        params["stream_options"] = {{"include_usage", options.stream_options->include_usage}};
    }
}

} // namespace LLMEngine

