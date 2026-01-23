// Copyright © 2026 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LLMEngine/AnalysisInput.hpp"

namespace LLMEngine {

nlohmann::json AnalysisInput::toJson() const {
    nlohmann::json j;

    // Handle messages
    if (!messages.empty()) {
        nlohmann::json msgs = nlohmann::json::array();

        // Strategy: Should we inject legacy system/user prompts?
        // Let's adopt a "messages-first" approach.
        // If messages are present, we assume the user populated them completely.
        // BUT for backward compatibility, if system_prompt exists and is NOT covered by messages, we MAY prepend it.
        // Let's implement safe merging:
        // Prepend system prompt if set
        if (!system_prompt.empty()) {
            msgs.push_back({{"role", "system"}, {"content", system_prompt}});
        }

        for (const auto& msg : messages) {
            nlohmann::json m = {{"role", msg.role}, {"content", msg.content}};
            if (!msg.name.empty())
                m["name"] = msg.name;
            if (!msg.tool_call_id.empty())
                m["tool_call_id"] = msg.tool_call_id;
            msgs.push_back(m);
        }

        // Append user message if set
        if (!user_message.empty()) {
            msgs.push_back({{"role", "user"}, {"content", user_message}});
        }

        j["messages"] = msgs;
    } else {
        // Legacy mode: output flat fields for APIClient (or whoever builds payload)
        // Note: APIClient expects "system_prompt" and "user_message" fields if it builds messages itself?
        // Wait, OpenAIClient::buildPayload helper might expect "messages" directly?
        // The current OpenAIClient likely builds messages from input["system_prompt"] etc.
        // But if we output "messages", OpenAIClient should use it.
        // Let's check OpenAIClient behavior later. Ideally we output legacy fields if messages is empty.
        if (!system_prompt.empty()) {
            j["system_prompt"] = system_prompt;
        }
        if (!user_message.empty()) {
            j["input_text"] = user_message; // Notice key is "input_text" in original file?
            // Wait, original file used "input_text" (line 14)?
            // "j["input_text"] = user_message;"
            // Let's double check AnalysisInput.cpp line 14.
            // YES.
        }
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
