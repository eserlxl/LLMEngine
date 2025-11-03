// Copyright © 2025 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#include "DebugArtifacts.hpp"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <algorithm>

namespace {
    constexpr size_t REDACTED_TAG_LENGTH = 10; // "<REDACTED>"
    constexpr size_t MIN_TOKEN_LENGTH_FOR_REDACTION = 32; // Minimum length for token redaction
}

void DebugArtifacts::writeJson(const std::string& path, const nlohmann::json& json, bool redactSecrets) {
    try {
        std::filesystem::path dir_path = std::filesystem::path(path).parent_path();
        if (!dir_path.empty()) {
            std::filesystem::create_directories(dir_path);
            // Set directory permissions to 0700 (owner-only read/write/execute) for security
            std::filesystem::permissions(dir_path, 
                std::filesystem::perms::owner_read | std::filesystem::perms::owner_write | std::filesystem::perms::owner_exec,
                std::filesystem::perm_options::replace);
        }
        nlohmann::json payload = redactSecrets ? redactJson(json) : json;
        std::ofstream f(path);
        if (!f) return;
        f << payload.dump(2);
    } catch (...) {
        // best-effort, swallow
        // NOLINTNEXTLINE(bugprone-empty-catch)
    }
}

void DebugArtifacts::writeText(const std::string& path, std::string_view text, bool redactSecrets) {
    try {
        std::filesystem::path dir_path = std::filesystem::path(path).parent_path();
        if (!dir_path.empty()) {
            std::filesystem::create_directories(dir_path);
            // Set directory permissions to 0700 (owner-only read/write/execute) for security
            std::filesystem::permissions(dir_path, 
                std::filesystem::perms::owner_read | std::filesystem::perms::owner_write | std::filesystem::perms::owner_exec,
                std::filesystem::perm_options::replace);
        }
        std::ofstream f(path);
        if (!f) return;
        if (redactSecrets) {
            std::string masked = redactText(text);
            f << masked;
        } else {
            f << text;
        }
    } catch (...) {
        // best-effort, swallow
        // NOLINTNEXTLINE(bugprone-empty-catch)
    }
}

void DebugArtifacts::cleanupOld(const std::string& dir, int hours) {
    if (hours <= 0) return;
    try {
        namespace fs = std::filesystem;
        if (!fs::exists(dir)) return;
        const auto now = std::chrono::system_clock::now();
        const auto retention = std::chrono::hours(hours);
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            const auto ft = entry.last_write_time();
            const auto ftp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                ft - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
            if ((now - ftp) > retention) {
                std::error_code ec;
                fs::remove(entry.path(), ec);
            }
        }
    } catch (...) {
        // best-effort, swallow
        // NOLINTNEXTLINE(bugprone-empty-catch)
    }
}

nlohmann::json DebugArtifacts::redactJson(const nlohmann::json& j) {
    if (j.is_string()) {
        return nlohmann::json{redactText(j.get<std::string>())};
    }
    if (!j.is_object()) return j;
    nlohmann::json out = j;
    static const std::vector<std::string> sensitive_keys = {
        "api_key","apikey","access_token","accesstoken","token",
        "authorization","x-api-key","xapikey","secret","password","passwd","pwd","credential"
    };
    for (auto& [key, value] : out.items()) {
        std::string lower = key;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (std::any_of(sensitive_keys.begin(), sensitive_keys.end(), [&](const std::string& s){ return lower.find(s) != std::string::npos; })) {
            out[key] = "<REDACTED>";
            continue;
        }
        if (value.is_object()) {
            out[key] = redactJson(value);
        } else if (value.is_array()) {
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& it : value) arr.push_back(redactJson(it));
            out[key] = arr;
        } else if (value.is_string()) {
            out[key] = redactText(value.get<std::string>());
        }
    }
    return out;
}

std::string DebugArtifacts::redactText(std::string_view text) {
    // Conservative masking: mask bearer tokens that look like long base64/hex strings
    std::string s(text);
    // Heuristic: any contiguous token chars of length >= MIN_TOKEN_LENGTH_FOR_REDACTION become <REDACTED>
    size_t i = 0;
    while (i < s.size()) {
        if (std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '_' || s[i] == '-' ) {
            size_t j = i;
            while (j < s.size() && (std::isalnum(static_cast<unsigned char>(s[j])) || s[j] == '_' || s[j] == '-' )) {
                ++j;
            }
            if (j - i >= MIN_TOKEN_LENGTH_FOR_REDACTION) {
                s.replace(i, j - i, "<REDACTED>");
                i += REDACTED_TAG_LENGTH;
                continue;
            }
            i = j;
        } else {
            ++i;
        }
    }
    return s;
}


