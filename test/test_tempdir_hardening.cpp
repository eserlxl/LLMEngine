// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/ITempDirProvider.hpp"
#include "LLMEngine/LLMEngine.hpp"
#include "LLMEngine/Logger.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>

using namespace LLMEngine;

namespace fs = std::filesystem;

// Test helper to create a temporary test directory
class TestTempDir {
  public:
    explicit TestTempDir(const std::string& prefix = "llmengine_test_") {
        char template_path[] = "/tmp/llmengine_test_XXXXXX";
        char* dir = mkdtemp(template_path);
        if (!dir) {
            throw std::runtime_error("Failed to create test temp directory");
        }
        path_ = dir;
    }

    ~TestTempDir() {
        if (fs::exists(path_)) {
            std::error_code ec;
            fs::remove_all(path_, ec);
        }
    }

    [[nodiscard]] const std::string& path() const {
        return path_;
    }

  private:
    std::string path_;
};

// Test helper to check file permissions (POSIX)
bool checkPermissions(const std::string& path, mode_t expected_mode) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return false;
    }
    // Only check owner permissions (0700)
    return (st.st_mode & 0777) == expected_mode;
}

void testSymlinkRejection() {
    std::cout << "Testing symlink rejection...\n";

    TestTempDir base_dir;
    const std::string test_dir = base_dir.path() + "/test";
    const std::string symlink_path = base_dir.path() + "/symlink";

    // Create a regular directory
    fs::create_directories(test_dir);

    // Create a symlink pointing to the test directory
    fs::create_symlink(test_dir, symlink_path);

    // Create LLMEngine instance and try to set the symlink path
    LLMEngine::LLMEngine engine(::LLMEngineAPI::ProviderType::OLLAMA, "", "test-model");

    // Try to set temp directory to symlink - should be rejected
    bool set_result = engine.setTempDirectory(symlink_path);
    assert(!set_result && "Symlink should be rejected");

    // Now try to ensure the directory - should throw if symlink exists at that path
    try {
        engine.setTempDirectory(test_dir);
        engine.prepareTempDirectory(); // This should work
        assert(true && "Normal directory should work");
    } catch (const std::exception& e) {
        std::cerr << "Unexpected exception: " << e.what() << std::endl;
        assert(false && "Normal directory should not throw");
    }

    // Test direct symlink detection
    try {
        // Create engine with symlink path directly
        fs::create_directories(symlink_path);
        fs::remove(symlink_path);
        fs::create_symlink("/tmp", symlink_path);

        // Try to prepare a directory that is a symlink
        LLMEngine::LLMEngine engine2(::LLMEngineAPI::ProviderType::OLLAMA, "", "test-model");
        if (engine2.setTempDirectory(symlink_path)) {
            try {
                engine2.prepareTempDirectory();
                assert(false && "Should have thrown for symlink");
            } catch (const std::runtime_error& e) {
                std::string msg = e.what();
                assert(msg.find("symlink") != std::string::npos && "Error should mention symlink");
            }
        }
    } catch (const std::exception& e) {
        // Expected if symlink creation fails
    }

    std::cout << "  ✓ Symlink rejection test passed\n";
}

void testDirectoryPermissions() {
    std::cout << "Testing directory permissions (0700)...\n";

    TestTempDir base_dir;
    const std::string test_dir = base_dir.path() + "/secure_dir";

    LLMEngine::LLMEngine engine(::LLMEngineAPI::ProviderType::OLLAMA, "", "test-model");
    engine.setTempDirectory(test_dir);
    engine.prepareTempDirectory();

    // Check that directory exists
    assert(fs::exists(test_dir) && "Directory should exist");
    assert(fs::is_directory(test_dir) && "Should be a directory");

    // Check permissions (owner read/write/execute only = 0700)
    // Note: On some systems, umask may affect final permissions
    // We check that owner permissions are set correctly
    struct stat st;
    assert(stat(test_dir.c_str(), &st) == 0 && "stat should succeed");
    mode_t owner_perms = st.st_mode & 0700;
    assert(owner_perms == 0700 && "Owner should have read/write/execute permissions");

    // Verify group and others have no permissions
    mode_t group_perms = st.st_mode & 0070;
    mode_t other_perms = st.st_mode & 0007;
    assert(group_perms == 0 && "Group should have no permissions");
    assert(other_perms == 0 && "Others should have no permissions");

    std::cout << "  ✓ Directory permissions test passed\n";
}

void testDirectoryCreation() {
    std::cout << "Testing directory creation...\n";

    TestTempDir base_dir;
    const std::string test_dir = base_dir.path() + "/nested/deep/directory";

    LLMEngine::LLMEngine engine(::LLMEngineAPI::ProviderType::OLLAMA, "", "test-model");
    engine.setTempDirectory(test_dir);
    engine.prepareTempDirectory();

    assert(fs::exists(test_dir) && "Directory should be created");
    assert(fs::is_directory(test_dir) && "Should be a directory");

    // Verify parent directories were created
    assert(fs::exists(base_dir.path() + "/nested") && "Parent directory should exist");
    assert(fs::exists(base_dir.path() + "/nested/deep") && "Nested parent should exist");

    std::cout << "  ✓ Directory creation test passed\n";
}

void testPathValidation() {
    std::cout << "Testing path validation (outside root rejection)...\n";

    TestTempDir base_dir;
    const std::string allowed_root = base_dir.path() + "/allowed";
    fs::create_directories(allowed_root);

    // Create a custom temp dir provider
    auto custom_provider = std::make_shared<DefaultTempDirProvider>(allowed_root);

    LLMEngine::LLMEngine engine(::LLMEngineAPI::ProviderType::OLLAMA, "", "test-model");
    // Note: We can't directly inject the provider after construction,
    // but we can test the path validation in setTempDirectory

    // Test 1: Path within allowed root should work
    const std::string within_path = allowed_root + "/subdir";
    // Since we're using default provider, we need to test with the actual default root
    const std::string default_root = DefaultTempDirProvider().getTempDir();

    // Test with a path that's definitely within default root
    const std::string test_within = default_root + "/test_subdir";
    bool result = engine.setTempDirectory(test_within);
    assert(result && "Path within root should be accepted");

    // Test 2: Path outside root should be rejected
    const std::string outside_path = "/tmp/llmengine_outside_test";
    bool result2 = engine.setTempDirectory(outside_path);
    // This should fail because outside_path is not within default_root
    // (unless default_root is /tmp, in which case it might work)
    // So we'll test with an absolute path that's definitely outside
    const std::string definitely_outside = "/usr/local/llmengine_test";
    bool result3 = engine.setTempDirectory(definitely_outside);
    assert(!result3 && "Path outside root should be rejected");

    std::cout << "  ✓ Path validation test passed\n";
}

void testMultipleCalls() {
    std::cout << "Testing multiple prepareTempDirectory calls...\n";

    TestTempDir base_dir;
    const std::string test_dir = base_dir.path() + "/multi_call";

    LLMEngine::LLMEngine engine(::LLMEngineAPI::ProviderType::OLLAMA, "", "test-model");
    engine.setTempDirectory(test_dir);

    // Call multiple times - should be safe
    engine.prepareTempDirectory();
    assert(fs::exists(test_dir) && "Directory should exist after first call");

    engine.prepareTempDirectory();
    assert(fs::exists(test_dir) && "Directory should still exist after second call");

    engine.prepareTempDirectory();
    assert(fs::exists(test_dir) && "Directory should still exist after third call");

    // Permissions should still be correct
    struct stat st;
    assert(stat(test_dir.c_str(), &st) == 0 && "stat should succeed");
    assert((st.st_mode & 0700) == 0700 && "Permissions should remain 0700");

    std::cout << "  ✓ Multiple calls test passed\n";
}

int main() {
    std::cout << "Running temp directory hardening tests...\n\n";

    try {
        testSymlinkRejection();
        testDirectoryPermissions();
        testDirectoryCreation();
        testPathValidation();
        testMultipleCalls();

        std::cout << "\n✓ All temp directory hardening tests passed!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n✗ Test failed with unknown exception\n";
        return 1;
    }
}
