// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/utils/ITempDirProvider.hpp"
#include "LLMEngine/core/LLMEngine.hpp"
#include "LLMEngine/utils/Logger.hpp"

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
    explicit TestTempDir([[maybe_unused]] const std::string& prefix = "llmengine_test_") {
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

    // Use paths within the default temp root to pass validation
    const std::string default_root = DefaultTempDirProvider().getTempDir();
    const std::string test_dir = default_root + "/test";
    const std::string symlink_path = default_root + "/symlink";

    // Cleanup from previous runs
    std::error_code ec;
    if (fs::exists(symlink_path))
        fs::remove(symlink_path, ec);
    if (fs::exists(test_dir))
        fs::remove_all(test_dir, ec);

    // Create a regular directory
    fs::create_directories(test_dir);

    // Create a symlink pointing to the test directory
    fs::create_symlink(test_dir, symlink_path);

    // Create LLMEngine instance and try to set the symlink path
    ::LLMEngine::LLMEngine engine(::LLMEngineAPI::ProviderType::OLLAMA, "", "test-model");

    // Try to set temp directory to symlink - should be rejected by symlink detection
    // Note: setTempDirectory may accept it (path validation passes), but prepareTempDirectory
    // should reject it
    bool set_result = engine.setTempDirectory(symlink_path);
    if (set_result) {
        // If path validation passed, prepareTempDirectory should detect and reject the symlink
        try {
            engine.prepareTempDirectory();
            assert(false && "Should have thrown for symlink");
        } catch (const std::runtime_error& e) {
            std::string msg = e.what();
            assert(msg.find("symlink") != std::string::npos && "Error should mention symlink");
        }
    } else {
        // Path validation may reject it if symlink resolution causes issues
        assert(true && "Symlink rejected by path validation");
    }

    // Now try to ensure the directory - should work for normal directory
    try {
        bool set_result2 = engine.setTempDirectory(test_dir);
        assert(set_result2 && "Normal directory within root should be accepted");
        (void)set_result2;             // Suppress unused variable warning - value checked in assert
        engine.prepareTempDirectory(); // This should work
        assert(true && "Normal directory should work");
    } catch (const std::exception& e) {
        std::cerr << "Unexpected exception: " << e.what() << std::endl;
        assert(false && "Normal directory should not throw");
    }

    // Test direct symlink detection with a symlink within the root
    try {
        const std::string symlink_path2 = default_root + "/symlink2";
        // Remove if exists
        if (fs::exists(symlink_path2)) {
            fs::remove(symlink_path2);
        }
        fs::create_symlink("/tmp", symlink_path2);

        // Try to prepare a directory that is a symlink
        ::LLMEngine::LLMEngine engine2(::LLMEngineAPI::ProviderType::OLLAMA, "", "test-model");
        bool set_result_symlink = engine2.setTempDirectory(symlink_path2);
        if (set_result_symlink) {
            try {
                engine2.prepareTempDirectory();
                assert(false && "Should have thrown for symlink");
            } catch (const std::runtime_error& e) {
                std::string msg = e.what();
                assert(msg.find("symlink") != std::string::npos && "Error should mention symlink");
            }
        }
        // Cleanup
        if (fs::exists(symlink_path2)) {
            fs::remove(symlink_path2);
        }
    } catch (const std::exception& e) {
        // Expected if symlink creation fails
    }

    // Cleanup artifacts
    if (fs::exists(symlink_path))
        fs::remove(symlink_path, ec);
    if (fs::exists(test_dir))
        fs::remove_all(test_dir, ec);

    std::cout << "  ✓ Symlink rejection test passed\n";
}

void testDirectoryPermissions() {
    std::cout << "Testing directory permissions (0700)...\n";

    // Use a path within the default temp root to pass validation
    const std::string default_root = DefaultTempDirProvider().getTempDir();
    const std::string test_dir = default_root + "/test_secure_dir";

    ::LLMEngine::LLMEngine engine(::LLMEngineAPI::ProviderType::OLLAMA, "", "test-model");
    bool set_result = engine.setTempDirectory(test_dir);
    assert(set_result && "Path within default root should be accepted");
    (void)set_result; // Suppress unused variable warning - value checked in assert
    engine.prepareTempDirectory();

    // Check that directory exists
    assert(fs::exists(test_dir) && "Directory should exist");
    assert(fs::is_directory(test_dir) && "Should be a directory");

    // Check permissions (owner read/write/execute only = 0700)
    // Note: On some systems, umask may affect final permissions
    // We check that owner permissions are set correctly
    struct stat st = {};
    assert(stat(test_dir.c_str(), &st) == 0 && "stat should succeed");
    mode_t owner_perms = st.st_mode & 0700;
    (void)owner_perms; // Suppress unused variable warning - value checked in assert
    assert(owner_perms == 0700 && "Owner should have read/write/execute permissions");

    // Verify group and others have no permissions
    mode_t group_perms = st.st_mode & 0070;
    mode_t other_perms = st.st_mode & 0007;
    (void)group_perms; // Suppress unused variable warning - value checked in assert
    (void)other_perms; // Suppress unused variable warning - value checked in assert
    assert(group_perms == 0 && "Group should have no permissions");
    assert(other_perms == 0 && "Others should have no permissions");

    std::cout << "  ✓ Directory permissions test passed\n";
}

void testDirectoryCreation() {
    std::cout << "Testing directory creation...\n";

    // Use a path within the default temp root to pass validation
    const std::string default_root = DefaultTempDirProvider().getTempDir();
    const std::string test_dir = default_root + "/nested/deep/directory";

    ::LLMEngine::LLMEngine engine(::LLMEngineAPI::ProviderType::OLLAMA, "", "test-model");
    bool set_result = engine.setTempDirectory(test_dir);
    assert(set_result && "Path within default root should be accepted");
    (void)set_result; // Suppress unused variable warning - value checked in assert
    engine.prepareTempDirectory();

    assert(fs::exists(test_dir) && "Directory should be created");
    assert(fs::is_directory(test_dir) && "Should be a directory");

    // Verify parent directories were created
    assert(fs::exists(default_root + "/nested") && "Parent directory should exist");
    assert(fs::exists(default_root + "/nested/deep") && "Nested parent should exist");

    std::cout << "  ✓ Directory creation test passed\n";
}

void testPathValidation() {
    std::cout << "Testing path validation (outside root rejection)...\n";

    TestTempDir base_dir;
    const std::string allowed_root = base_dir.path() + "/allowed";
    fs::create_directories(allowed_root);

    // Create a custom temp dir provider
    auto custom_provider = std::make_shared<DefaultTempDirProvider>(allowed_root);

    ::LLMEngine::LLMEngine engine(::LLMEngineAPI::ProviderType::OLLAMA, "", "test-model");
    // Note: We can't directly inject the provider after construction,
    // but we can test the path validation in setTempDirectory

    // Test 1: Path within allowed root should work
    const std::string within_path = allowed_root + "/subdir";
    // Since we're using default provider, we need to test with the actual default root
    const std::string default_root = DefaultTempDirProvider().getTempDir();

    // Test with a path that's definitely within default root
    const std::string test_within = default_root + "/test_subdir";
    bool result = engine.setTempDirectory(test_within);
    (void)result; // Suppress unused variable warning - value checked in assert
    assert(result && "Path within root should be accepted");

    // Test 2: Path outside root should be rejected
    const std::string outside_path = "/tmp/llmengine_outside_test";
    bool result2 = engine.setTempDirectory(outside_path);
    (void)result2; // Suppress unused variable warning - value checked in assert
    // This should fail because outside_path is not within default_root
    // (unless default_root is /tmp, in which case it might work)
    // So we'll test with an absolute path that's definitely outside
    const std::string definitely_outside = "/usr/local/llmengine_test";
    bool result3 = engine.setTempDirectory(definitely_outside);
    (void)result3; // Suppress unused variable warning - value checked in assert
    assert(!result3 && "Path outside root should be rejected");

    std::cout << "  ✓ Path validation test passed\n";
}

void testMultipleCalls() {
    std::cout << "Testing multiple prepareTempDirectory calls...\n";

    // Use a path within the default temp root to pass validation
    const std::string default_root = DefaultTempDirProvider().getTempDir();
    const std::string test_dir = default_root + "/multi_call";

    ::LLMEngine::LLMEngine engine(::LLMEngineAPI::ProviderType::OLLAMA, "", "test-model");
    bool set_result = engine.setTempDirectory(test_dir);
    if (!set_result) {
        std::cerr << "Error: Failed to set temp directory to " << test_dir << std::endl;
    }
    assert(set_result && "Path within default root should be accepted");

    // Call multiple times - should be safe
    engine.prepareTempDirectory();
    assert(fs::exists(test_dir) && "Directory should exist after first call");

    engine.prepareTempDirectory();
    assert(fs::exists(test_dir) && "Directory should still exist after second call");

    engine.prepareTempDirectory();
    assert(fs::exists(test_dir) && "Directory should still exist after third call");

    // Permissions should still be correct
    struct stat st = {};
    assert(stat(test_dir.c_str(), &st) == 0 && "stat should succeed");
    mode_t final_perms = st.st_mode & 0700;
    assert(final_perms == 0700 && "Permissions should remain 0700");
    (void)st;          // Explicitly mark as used (used in final_perms calculation)
    (void)final_perms; // Suppress unused variable warning - value checked in assert

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
