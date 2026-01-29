// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LLMEngine/utils/CommandExecutor.hpp"
#include <iostream>
#include <vector>
#include <string>

using namespace LLMEngine::Utils;

void testEcho() {
    std::cout << "Testing echo..." << std::endl;
    // Note: execCommand splits by space. "echo hello" -> ["echo", "hello"]
    auto result = execCommand("echo hello");
    if (!result.empty() && result[0] == "hello") {
        std::cout << "PASS: echo hello" << std::endl;
    } else {
        std::cout << "FAIL: echo hello. Output size: " << result.size() << std::endl;
        for (const auto& line : result) {
            std::cout << "Line: " << line << std::endl;
        }
    }
}

void testInvalidChars() {
    std::cout << "Testing invalid chars..." << std::endl;
    auto result = execCommand("echo hello; rm -rf /"); // Should be rejected by regex
    if (result.empty()) {
        std::cout << "PASS: Rejected unsafe command" << std::endl;
    } else {
        std::cout << "FAIL: Accepted unsafe command!" << std::endl;
    }
}

int main() {
    std::cout << "=== CommandExecutor Test ===" << std::endl;
    testEcho();
    testInvalidChars();
    std::cout << "=== End CommandExecutor Test ===" << std::endl;
    return 0;
}
