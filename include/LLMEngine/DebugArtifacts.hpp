// Copyright © 2025 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <string>
#include <string_view>
#include <nlohmann/json.hpp>

class DebugArtifacts {
public:
    // Write JSON to path. When redactSecrets is true, common secret fields are masked.
    static void writeJson(const std::string& path, const nlohmann::json& json, bool redactSecrets);

    // Write text to path. When redactSecrets is true, token-like substrings are masked conservatively.
    static void writeText(const std::string& path, std::string_view text, bool redactSecrets);

    // Remove files older than the given hours in dir. No-throw best-effort.
    static void cleanupOld(const std::string& dir, int hours);

private:
    static nlohmann::json redactJson(const nlohmann::json& j);
    static std::string redactText(std::string_view text);
};



