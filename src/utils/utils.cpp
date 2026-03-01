// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "llmengine/utils/utils.hpp"

#include <algorithm>
#include <cctype> // for iscntrl
#include <fstream>
#include <regex>
#include <string>
#include <vector>



namespace LLMEngine::Utils {


// Constants
// Validation constants

// Validation constants
constexpr size_t minApiKeyLength = 10;
constexpr size_t maxApiKeyLength = 512;
constexpr size_t maxModelNameLength = 256;
constexpr size_t maxUrlLength = 2048;
constexpr size_t minUrlLength = 7;      // Minimum "http://"
constexpr size_t httpPrefixLength = 7;  // "http://"
constexpr size_t httpsPrefixLength = 8; // "https://"

// Precompiled regex patterns
static const std::regex safeCharsRegex(R"([a-zA-Z0-9_./ -]+)");
static const std::regex markdownBoldRegex(R"(\*\*)");
static const std::regex markdownHeaderRegex(R"(#+\s*)");

// Platform-specific close helper


std::vector<std::string> readLines(std::string_view filepath, size_t maxLines) {
    std::vector<std::string> lines;
    lines.reserve(maxLines);
    std::ifstream file{std::string(filepath)};
    std::string line;
    while (std::getline(file, line) && lines.size() < maxLines) {
        lines.push_back(line);
    }
    return lines;
}



std::string stripMarkdown(std::string_view input) {
    std::string output = std::string(input);
    output = std::regex_replace(output, markdownBoldRegex, "");
    output = std::regex_replace(output, markdownHeaderRegex, "");
    return output;
}

bool validateApiKey(std::string_view apiKey) {
    if (apiKey.empty()) {
        return false;
    }

    // Check length (reasonable bounds)
    if (apiKey.size() < minApiKeyLength || apiKey.size() > maxApiKeyLength) {
        return false;
    }

    // Check for control characters
    if (!std::ranges::all_of(apiKey,
                             [](char c) { return !std::iscntrl(static_cast<unsigned char>(c)); })) {
        return false;
    }

    return true;
}

bool validateModelName(std::string_view modelName) {
    if (modelName.empty()) {
        return false;
    }

    // Check length
    if (modelName.size() > maxModelNameLength) {
        return false;
    }

    // Check for allowed characters: alphanumeric, hyphens, underscores, dots,
    // slashes
    if (!std::ranges::all_of(modelName, [](char c) {
            return std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.'
                   || c == '/';
        })) {
        return false;
    }

    return true;
}

bool validateUrl(std::string_view url) {
    if (url.empty()) {
        return false;
    }

    // Check length
    if (url.size() > maxUrlLength) {
        return false;
    }

    // Must start with http:// or https://
    if (url.size() < minUrlLength) {
        return false;
    }

    if (url.substr(0, httpPrefixLength) != "http://"
        && url.substr(0, httpsPrefixLength) != "https://") {
        return false;
    }

    // Check for control characters
    if (!std::ranges::all_of(url,
                             [](char c) { return !std::iscntrl(static_cast<unsigned char>(c)); })) {
        return false;
    }

    return true;
}

std::string base64Encode(std::span<const uint8_t> data) {
    static const char base64Chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);

    constexpr uint32_t byteShift1 = 16;
    constexpr uint32_t byteShift2 = 8;
    constexpr uint32_t charShift1 = 18;
    constexpr uint32_t charShift2 = 12;
    constexpr uint32_t charShift3 = 6;
    constexpr uint32_t charMask = 0x3F;

    for (size_t i = 0; i < data.size(); i += 3) {
        uint32_t val = static_cast<uint32_t>(data[i]) << byteShift1;
        if (i + 1 < data.size())
            val |= static_cast<uint32_t>(data[i + 1]) << byteShift2;
        if (i + 2 < data.size())
            val |= static_cast<uint32_t>(data[i + 2]);

        out.push_back(base64Chars[(val >> charShift1) & charMask]);
        out.push_back(base64Chars[(val >> charShift2) & charMask]);

        if (i + 1 < data.size())
            out.push_back(base64Chars[(val >> charShift3) & charMask]);
        else
            out.push_back('=');

        if (i + 2 < data.size())
            out.push_back(base64Chars[val & charMask]);
        else
            out.push_back('=');
    }
    return out;
}

std::string base64Encode(std::string_view data) {
    return base64Encode(
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(data.data()), data.size()));
}

} // namespace LLMEngine::Utils
