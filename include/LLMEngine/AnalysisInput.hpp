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

    struct ContentPart {
        enum class Type { Text, ImageUrl };
        Type type;
        std::string text;      // For Text
        std::string image_url; // For ImageUrl (base64 or http)

        static ContentPart createText(std::string_view text) {
            return ContentPart{Type::Text, std::string(text), ""};
        }
        static ContentPart createImage(std::string_view url) {
            return ContentPart{Type::ImageUrl, "", std::string(url)};
        }
    };

    struct ChatMessage {
        std::string role;
        std::vector<ContentPart> parts;
        std::string name;         // Optional
        std::string tool_call_id; // Optional

        // Helper to get simple text content
        std::string getTextContent() const {
             std::string combined;
             for (const auto& part : parts) {
                 if (part.type == ContentPart::Type::Text) combined += part.text;
             }
             return combined;
        }
    };
    std::vector<ChatMessage> messages;

    // Builder Pattern
    static AnalysisInput builder() {
        return AnalysisInput{};
    }

    AnalysisInput& withMessage(std::string_view role, std::string_view content) {
        messages.push_back({std::string(role), {ContentPart::createText(content)}, "", ""});
        return *this;
    }

    AnalysisInput& withMessage(std::string_view role, const std::vector<ContentPart>& parts) {
        messages.push_back({std::string(role), parts, "", ""});
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

    /**
     * @brief Load image from file, base64 encode it, and add to input.
     * @param path Path to image file.
     * @throws std::runtime_error if file cannot be read.
     */
    AnalysisInput& withImageFromFile(const std::string& path);

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
