// Copyright © 2026 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LLMEngine/AnalysisInput.hpp"

#include "LLMEngine/AnalysisInput.hpp"
#include "LLMEngine/Utils.hpp"
#include <filesystem>
#include <fstream>
#include <vector>

namespace LLMEngine {

AnalysisInput& AnalysisInput::withImageFromFile(const std::string& path) {
    std::filesystem::path p(path);
    if (!std::filesystem::exists(p)) {
        throw std::runtime_error("Image file not found: " + path);
    }

    std::ifstream file(p, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Could not open image file: " + path);
    }

    auto size = file.tellg();
    if (size <= 0) {
        throw std::runtime_error("Image file is empty: " + path);
    }

    std::vector<uint8_t> buffer(size);
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(buffer.data()), size);

    std::string base64 = Utils::base64Encode(std::span<const uint8_t>(buffer));
    // Determine MIME type? For now, we assume provider detects or format is base64 string directly.
    // OpenAI expects "data:image/jpeg;base64,{base64}" usually.
    // Anthropic expects base64 string directly with media_type field separate.
    // Current design stores strings in `images` vector.
    // `AnalysisInput::toJson` just dumps `images` array.
    // `ChatMessageBuilder` handles `images` array.
    // Looking at `ChatCompletionRequestHelper.hpp`:
    // user_content.push_back({{"type", "image_url"}, {"image_url", {{"url", img.get<std::string>()}}}});
    // Providers accepting base64 usually want a data URI for "url" field.
    
    // Simple MIME detection by extension
    std::string ext = p.extension().string();
    if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
    
    std::string mime = "image/jpeg"; // Default
    if (ext == "png") mime = "image/png";
    else if (ext == "gif") mime = "image/gif";
    else if (ext == "webp") mime = "image/webp";

    std::string dataUri = "data:" + mime + ";base64," + base64;
    images.push_back(dataUri);
    return *this;
}

nlohmann::json AnalysisInput::toJson() const {
    nlohmann::json j;

    // Unified logic: Build messages array from any combination of fields
    nlohmann::json msgs = nlohmann::json::array();

    // 1. Prepend legacy system prompt if set
    if (!system_prompt.empty()) {
        msgs.push_back({{"role", "system"}, {"content", system_prompt}});
    }

    // 2. Append explicit messages
    // 2. Append explicit messages
    for (const auto& msg : messages) {
        nlohmann::json m;
        m["role"] = msg.role;

        // Serialize content
        if (msg.parts.empty()) {
            m["content"] = "";
        } else if (msg.parts.size() == 1 && msg.parts[0].type == ContentPart::Type::Text) {
            // Optimization: Use simple string for single text part
            m["content"] = msg.parts[0].text;
        } else {
            // Multimodal: Use array of content parts
            nlohmann::json contentArr = nlohmann::json::array();
            for (const auto& part : msg.parts) {
                if (part.type == ContentPart::Type::Text) {
                    contentArr.push_back({{"type", "text"}, {"text", part.text}});
                } else if (part.type == ContentPart::Type::ImageUrl) {
                    contentArr.push_back({{"type", "image_url"}, {"image_url", {{"url", part.image_url}}}});
                }
            }
            m["content"] = contentArr;
        }

        if (!msg.name.empty())
            m["name"] = msg.name;
        if (!msg.tool_call_id.empty())
            m["tool_call_id"] = msg.tool_call_id;
        msgs.push_back(m);
    }

    // 3. Append legacy user message if set
    if (!user_message.empty()) {
        msgs.push_back({{"role", "user"}, {"content", user_message}});
    }

    if (!msgs.empty()) {
        j["messages"] = msgs;
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
