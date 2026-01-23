// Copyright © 2026 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LLMEngine/AnalysisInput.hpp"

namespace LLMEngine {

nlohmann::json AnalysisInput::toJson() const {
    nlohmann::json j;
    if (!system_prompt.empty()) {
        j["system_prompt"] = system_prompt;
    }
    if (!user_message.empty()) {
        j["input_text"] = user_message;
    }
    if (!images.empty()) {
        j["images"] = images;
    }
    if (!tools.is_null()) {
        j["tools"] = tools;
    }
    if (!tool_choice.is_null()) {
        j["tool_choice"] = tool_choice;
    }
    if (!response_format.is_null()) {
        j["response_format"] = response_format;
    }
    for (const auto& [key, value] : extra_fields) {
        j[key] = value;
    }
    return j;
}

} // namespace LLMEngine
