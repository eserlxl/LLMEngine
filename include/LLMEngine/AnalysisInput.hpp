// Copyright © 2026 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "LLMEngine/LLMEngineExport.hpp"
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace LLMEngine {

/**
 * @brief Strongly-typed builder for analysis inputs.
 *
 * Replaces unstructured JSON with a type-safe API for constructing
 * requests. Supports text, images, tools, and extra fields.
 */
struct LLMENGINE_EXPORT AnalysisInput {
    std::string system_prompt;
    std::string user_message;
    std::vector<std::string> images; // Base64 or URLs
    nlohmann::json tools;
    nlohmann::json tool_choice;
    nlohmann::json response_format;

    std::map<std::string, nlohmann::json> extra_fields;

    struct ChatMessage {
        std::string role;
        std::string content;
        std::string name;         // Optional
        std::string tool_call_id; // Optional
    };
    std::vector<ChatMessage> messages;

    // Builder Pattern
    static AnalysisInput builder() {
        return AnalysisInput{};
    }

    AnalysisInput& withMessage(std::string_view role, std::string_view content) {
        messages.push_back({std::string(role), std::string(content), "", ""});
        return *this;
    }

    AnalysisInput& withMessages(const std::vector<ChatMessage>& msgs) {
        messages.insert(messages.end(), msgs.begin(), msgs.end());
        return *this;
    }

    AnalysisInput& withSystemPrompt(std::string_view prompt) {
        system_prompt = prompt;
        return *this;
    }

    AnalysisInput& withUserMessage(std::string_view message) {
        user_message = message;
        return *this;
    }

    AnalysisInput& withImage(std::string_view image_data) {
        images.emplace_back(image_data);
        return *this;
    }

    AnalysisInput& withTools(const nlohmann::json& tools_json) {
        tools = tools_json;
        return *this;
    }

    AnalysisInput& withToolChoice(const nlohmann::json& choice) {
        tool_choice = choice;
        return *this;
    }

    AnalysisInput& withResponseFormat(const nlohmann::json& format) {
        response_format = format;
        return *this;
    }

    template <typename T> AnalysisInput& withExtraField(const std::string& key, const T& value) {
        extra_fields[key] = value;
        return *this;
    }

    /**
     * @brief Convert to legacy JSON format for API compliance.
     */
    [[nodiscard]] nlohmann::json toJson() const;
};

} // namespace LLMEngine
