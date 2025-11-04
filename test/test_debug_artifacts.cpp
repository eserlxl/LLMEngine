// Copyright © 2025 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../src/DebugArtifacts.hpp"
#include "LLMEngine/Utils.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>

static std::string readAll(const std::string& path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

int main() {
    namespace fs = std::filesystem;
    const std::string baseDir = std::string(LLMEngine::Utils::TMP_DIR) + std::string("/llmengine_test_debug");
    std::error_code ec;
    fs::create_directories(baseDir, ec);

    // Test JSON redaction
    {
        nlohmann::json j = {
            {"api_key", "sk_abcdefghijklmnopqrstuvwxyz012345"},
            {"nested", {{"password", "hunter2"}, {"token", "tok_ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"}}},
            {"note", "this contains tok_ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 inline"}
        };
        const std::string p = baseDir + "/redact.json";
        DebugArtifacts::writeJson(p, j, /*redactSecrets*/true);
        std::string content = readAll(p);
        assert(content.find("sk_abcdefghijklmnopqrstuvwxyz012345") == std::string::npos);
        assert(content.find("hunter2") == std::string::npos);
        assert(content.find("tok_ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") == std::string::npos);
        assert(content.find("<REDACTED>") != std::string::npos);
    }

    // Test text redaction (long token masking)
    {
        std::string longToken(40, 'A');
        std::string text = std::string("prefix ") + longToken + " suffix";
        const std::string p = baseDir + "/redact.txt";
        DebugArtifacts::writeText(p, text, /*redactSecrets*/true);
        std::string content = readAll(p);
        assert(content.find(longToken) == std::string::npos);
        assert(content.find("<REDACTED>") != std::string::npos);
    }

    // Test cleanupOld (retention)
    {
        const std::string oldPath = baseDir + "/old.txt";
        const std::string newPath = baseDir + "/new.txt";
        {
            std::ofstream f(oldPath); f << "old";
            std::ofstream g(newPath); g << "new";
        }
        // Set old file mtime to 48 hours ago, new file to now
        const auto now = std::chrono::system_clock::now();
        const auto oldTime = now - std::chrono::hours(48);
        auto toFileClock = std::chrono::time_point_cast<std::filesystem::file_time_type::duration>(
            oldTime - std::chrono::system_clock::now() + std::filesystem::file_time_type::clock::now());
        std::filesystem::last_write_time(oldPath, toFileClock, ec);

        DebugArtifacts::cleanupOld(baseDir, /*hours*/24);

        bool oldExists = fs::exists(oldPath, ec);
        bool newExists = fs::exists(newPath, ec);
        assert(!oldExists);
        assert(newExists);
    }

    std::cout << "test_debug_artifacts: OK\n";
    return 0;
}


