// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "LLMEngine/Logger.hpp"
#include <string>
#include <memory>
#include <filesystem>

namespace LLMEngine::Test {

/**
 * @brief Common test context for standardized testing environment.
 */
struct TestContext {
    std::string testName;
    std::string tempDir;
    std::shared_ptr<Logger> logger;
};

/**
 * @brief Create a standardized test context.
 * @param name Unique name for the test suite/case.
 * @return TestContext with initialized logger and temp directory.
 */
inline TestContext createTestContext(const std::string& name) {
    TestContext ctx;
    ctx.testName = name;
    
    // Create a temporary directory path
    auto sysTmp = std::filesystem::temp_directory_path();
    ctx.tempDir = (sysTmp / "llmengine_tests" / name).string();
    
    // Ensure it exists
    std::error_code ec;
    std::filesystem::create_directories(ctx.tempDir, ec);
    
    ctx.logger = std::make_shared<DefaultLogger>();
    
    return ctx;
}

} // namespace LLMEngine::Test
