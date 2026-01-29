// Copyright © 2026 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#include "diagnostics/DebugArtifacts.hpp"
#include "LLMEngine/providers/APIClient.hpp"
#include "LLMEngine/diagnostics/DebugArtifactManager.hpp"
#include "LLMEngine/utils/Utils.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

static std::string readAll(const std::string& path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

int main() {
    namespace fs = std::filesystem;
    const std::string baseDir =
        std::string(LLMEngine::Utils::TMP_DIR) + std::string("/llmengine_test_debug");
    std::error_code ec;
    fs::create_directories(baseDir, ec);

    // Test JSON redaction
    {
        nlohmann::json j = {
            {"api_key", "sk_abcdefghijklmnopqrstuvwxyz012345"},
            {"nested",
             {{"password", "hunter2"}, {"token", "tok_ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"}}},
            {"note", "this contains tok_ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 inline"}};
        const std::string p = baseDir + "/redact.json";
        DebugArtifacts::writeJson(p, j, /*redactSecrets*/ true);
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
        DebugArtifacts::writeText(p, text, /*redactSecrets*/ true);
        std::string content = readAll(p);
        assert(content.find(longToken) == std::string::npos);
        assert(content.find("<REDACTED>") != std::string::npos);
    }

    // Test cleanupOld (retention)
    {
        const std::string oldPath = baseDir + "/old.txt";
        const std::string newPath = baseDir + "/new.txt";
        {
            std::ofstream f(oldPath);
            f << "old";
            std::ofstream g(newPath);
            g << "new";
        }
        // Set old file mtime to 48 hours ago, new file to now
        const auto now = std::chrono::system_clock::now();
        const auto oldTime = now - std::chrono::hours(48);
        auto toFileClock = std::chrono::time_point_cast<std::filesystem::file_time_type::duration>(
            oldTime - std::chrono::system_clock::now()
            + std::filesystem::file_time_type::clock::now());
        std::filesystem::last_write_time(oldPath, toFileClock, ec);

        DebugArtifacts::cleanupOld(baseDir, /*hours*/ 24);

        bool oldExists = fs::exists(oldPath, ec);
        bool newExists = fs::exists(newPath, ec);
        assert(!oldExists);
        assert(newExists);
    }

    // Test DebugArtifactManager
    {
        const std::string requestDir = baseDir + "/test_request_123";
        LLMEngine::DebugArtifactManager mgr(requestDir, baseDir, 24, nullptr);

        // Test ensureRequestDirectory
        assert(mgr.ensureRequestDirectory());
        assert(fs::exists(requestDir, ec));
        assert(mgr.getRequestTmpDir() == requestDir);

        // Test writeApiResponse (success)
        LLMEngineAPI::APIResponse success_response;
        success_response.success = true;
        success_response.content = "Test response";
        success_response.status_code = 200;
        success_response.raw_response = {{"choices", nlohmann::json::array()}};
        assert(mgr.writeApiResponse(success_response, false));

        const std::string apiResponsePath = requestDir + "/api_response.json";
        assert(fs::exists(apiResponsePath, ec));
        std::string content = readAll(apiResponsePath);
        assert(content.find("choices") != std::string::npos);

        // Test writeApiResponse (error)
        LLMEngineAPI::APIResponse error_response;
        error_response.success = false;
        error_response.error_message = "Test error";
        error_response.status_code = 500;
        error_response.raw_response = {{"error", "Internal server error"}};
        assert(mgr.writeApiResponse(error_response, true));

        const std::string apiErrorPath = requestDir + "/api_response_error.json";
        assert(fs::exists(apiErrorPath, ec));

        // Test writeFullResponse
        assert(mgr.writeFullResponse("This is a full response text"));
        const std::string fullResponsePath = requestDir + "/response_full.txt";
        assert(fs::exists(fullResponsePath, ec));
        content = readAll(fullResponsePath);
        assert(content.find("full response") != std::string::npos);

        // Test writeAnalysisArtifacts
        assert(mgr.writeAnalysisArtifacts("test_analysis", "Think section", "Content section"));
        const std::string thinkPath = requestDir + "/test_analysis_think.txt";
        const std::string contentPath = requestDir + "/test_analysis_content.txt";
        assert(fs::exists(thinkPath, ec));
        assert(fs::exists(contentPath, ec));
        assert(readAll(thinkPath).find("Think section") != std::string::npos);
        assert(readAll(contentPath).find("Content section") != std::string::npos);

        std::cout << "✓ DebugArtifactManager basic tests passed\n";
    }

    // Test DebugArtifactManager with sanitized analysis type
    {
        const std::string requestDir2 = baseDir + "/test_request_456";
        LLMEngine::DebugArtifactManager mgr2(requestDir2, baseDir, 24, nullptr);
        mgr2.ensureRequestDirectory();

        // Test with analysis type containing special characters
        assert(mgr2.writeAnalysisArtifacts("test/../analysis", "think", "content"));
        // Should sanitize to safe filename
        const std::string sanitizedPath = requestDir2 + "/test___analysis_think.txt";
        assert(fs::exists(sanitizedPath, ec)
               || fs::exists(requestDir2 + "/analysis_think.txt", ec));

        std::cout << "✓ DebugArtifactManager sanitization test passed\n";
    }

    // Test DebugArtifactManager with empty analysis type
    {
        const std::string requestDir3 = baseDir + "/test_request_789";
        LLMEngine::DebugArtifactManager mgr3(requestDir3, baseDir, 24, nullptr);
        mgr3.ensureRequestDirectory();

        assert(mgr3.writeAnalysisArtifacts("", "think", "content"));
        // Should default to "analysis"
        const std::string defaultPath = requestDir3 + "/analysis_think.txt";
        assert(fs::exists(defaultPath, ec));

        std::cout << "✓ DebugArtifactManager empty analysis type test passed\n";
    }

    std::cout << "test_debug_artifacts: OK\n";
    return 0;
}
