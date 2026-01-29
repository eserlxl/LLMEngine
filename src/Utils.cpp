// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/Utils.hpp"

#include "LLMEngine/Logger.hpp"

#include <algorithm>
#include <cctype> // for iscntrl
#include <filesystem>
#include <fstream>
#include <regex>
#include <string>
#include <vector>



namespace LLMEngine::Utils {
namespace fs = std::filesystem;

// Constants
// Validation constants

// Validation constants
constexpr size_t MIN_API_KEY_LENGTH = 10;
constexpr size_t MAX_API_KEY_LENGTH = 512;
constexpr size_t MAX_MODEL_NAME_LENGTH = 256;
constexpr size_t MAX_URL_LENGTH = 2048;
constexpr size_t MIN_URL_LENGTH = 7;      // Minimum "http://"
constexpr size_t HTTP_PREFIX_LENGTH = 7;  // "http://"
constexpr size_t HTTPS_PREFIX_LENGTH = 8; // "https://"

// Precompiled regex patterns
static const std::regex SAFE_CHARS_REGEX(R"([a-zA-Z0-9_./ -]+)");
static const std::regex MARKDOWN_BOLD_REGEX(R"(\*\*)");
static const std::regex MARKDOWN_HEADER_REGEX(R"(#+\s*)");

// Platform-specific close helper


std::vector<std::string> readLines(std::string_view filepath, size_t max_lines) {
    std::vector<std::string> lines;
    lines.reserve(max_lines);
    std::ifstream file{std::string(filepath)};
    std::string line;
    while (std::getline(file, line) && lines.size() < max_lines) {
        lines.push_back(line);
    }
    return lines;
}



std::string stripMarkdown(std::string_view input) {
    std::string output = std::string(input);
    output = std::regex_replace(output, MARKDOWN_BOLD_REGEX, "");
    output = std::regex_replace(output, MARKDOWN_HEADER_REGEX, "");
    return output;
}

bool validateApiKey(std::string_view api_key) {
    if (api_key.empty()) {
        return false;
    }

    // Check length (reasonable bounds)
    if (api_key.size() < MIN_API_KEY_LENGTH || api_key.size() > MAX_API_KEY_LENGTH) {
        return false;
    }

    // Check for control characters
    if (!std::ranges::all_of(api_key,
                             [](char c) { return !std::iscntrl(static_cast<unsigned char>(c)); })) {
        return false;
    }

    return true;
}

bool validateModelName(std::string_view model_name) {
    if (model_name.empty()) {
        return false;
    }

    // Check length
    if (model_name.size() > MAX_MODEL_NAME_LENGTH) {
        return false;
    }

    // Check for allowed characters: alphanumeric, hyphens, underscores, dots,
    // slashes
    if (!std::ranges::all_of(model_name, [](char c) {
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
    if (url.size() > MAX_URL_LENGTH) {
        return false;
    }

    // Must start with http:// or https://
    if (url.size() < MIN_URL_LENGTH) {
        return false;
    }

    if (url.substr(0, HTTP_PREFIX_LENGTH) != "http://"
        && url.substr(0, HTTPS_PREFIX_LENGTH) != "https://") {
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

    for (size_t i = 0; i < data.size(); i += 3) {
        uint32_t val = data[i] << 16;
        if (i + 1 < data.size())
            val |= data[i + 1] << 8;
        if (i + 2 < data.size())
            val |= data[i + 2];

        out.push_back(base64Chars[(val >> 18) & 0x3F]);
        out.push_back(base64Chars[(val >> 12) & 0x3F]);

        if (i + 1 < data.size())
            out.push_back(base64Chars[(val >> 6) & 0x3F]);
        else
            out.push_back('=');

        if (i + 2 < data.size())
            out.push_back(base64Chars[val & 0x3F]);
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
