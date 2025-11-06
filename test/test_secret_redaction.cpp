// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of the LLMEngine test suite (Secret Redaction tests) and is
// licensed under the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/Logger.hpp"
#include "LLMEngine/RequestLogger.hpp"
#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

// Test logger that captures output
class TestLogger : public LLMEngine::Logger {
public:
    void log(LLMEngine::LogLevel level, std::string_view message) override {
        last_message_ = std::string(message);
        last_level_ = level;
    }

    std::string last_message_;
    LLMEngine::LogLevel last_level_;
};

void testRedactTextBasic() {
    std::string input = "api_key=sk_abcdefghijklmnopqrstuvwxyz012345";
    std::string result = LLMEngine::RequestLogger::redactText(input);

    assert(result.find("sk_abcdefghijklmnopqrstuvwxyz012345") == std::string::npos);
    assert(result.find("<REDACTED>") != std::string::npos);
    assert(result.find("api_key") != std::string::npos);

    std::cout << "✓ Basic text redaction test passed\n";
}

void testRedactTextMultipleSecrets() {
    std::string input = "api_key=sk_test123 token=abc123 secret=xyz789";
    std::string result = LLMEngine::RequestLogger::redactText(input);

    assert(result.find("sk_test123") == std::string::npos);
    assert(result.find("abc123") == std::string::npos);
    assert(result.find("xyz789") == std::string::npos);
    assert(result.find("<REDACTED>") != std::string::npos);

    std::cout << "✓ Multiple secrets redaction test passed\n";
}

void testRedactTextColonFormat() {
    std::string input = "Authorization: Bearer sk_abcdefghijklmnopqrstuvwxyz012345";
    std::string result = LLMEngine::RequestLogger::redactText(input);

    assert(result.find("sk_abcdefghijklmnopqrstuvwxyz012345") == std::string::npos);
    assert(result.find("<REDACTED>") != std::string::npos);

    std::cout << "✓ Colon format redaction test passed\n";
}

void testRedactUrl() {
    std::string url = "https://api.example.com/v1/chat?api_key=sk_test123&model=gpt-4";
    std::string result = LLMEngine::RequestLogger::redactUrl(url);

    assert(result.find("sk_test123") == std::string::npos);
    assert(result.find("api_key=<REDACTED>") != std::string::npos);
    assert(result.find("model=gpt-4") != std::string::npos); // Non-sensitive param should remain

    std::cout << "✓ URL redaction test passed\n";
}

void testRedactHeaders() {
    std::map<std::string, std::string> headers = {
        {"Authorization", "Bearer sk_abcdefghijklmnopqrstuvwxyz012345"},
        {"Content-Type", "application/json"},
        {"X-API-Key", "test_key_12345"}};

    auto redacted = LLMEngine::RequestLogger::redactHeaders(headers);

    assert(redacted["Authorization"] == "<REDACTED>");
    assert(redacted["X-API-Key"] == "<REDACTED>");
    assert(redacted["Content-Type"] == "application/json"); // Non-sensitive header should remain

    std::cout << "✓ Header redaction test passed\n";
}

void testLogSafe() {
    TestLogger logger;
    std::string message = "Error: api_key=sk_abcdefghijklmnopqrstuvwxyz012345";

    LLMEngine::RequestLogger::logSafe(&logger, LLMEngine::LogLevel::Error, message);

    assert(logger.last_level_ == LLMEngine::LogLevel::Error);
    assert(logger.last_message_.find("sk_abcdefghijklmnopqrstuvwxyz012345") == std::string::npos);
    assert(logger.last_message_.find("<REDACTED>") != std::string::npos);

    std::cout << "✓ Safe logging test passed\n";
}

void testLogSafeNullLogger() {
    // Should not crash with null logger
    LLMEngine::RequestLogger::logSafe(nullptr, LLMEngine::LogLevel::Error, "test message");

    std::cout << "✓ Null logger safe logging test passed\n";
}

void testRedactTextNoSecrets() {
    std::string input = "This is a normal message with no secrets";
    std::string result = LLMEngine::RequestLogger::redactText(input);

    assert(result == input); // Should remain unchanged

    std::cout << "✓ No secrets redaction test passed\n";
}

void testRedactTextQuotedValues() {
    std::string input = "api_key=\"sk_abcdefghijklmnopqrstuvwxyz012345\"";
    std::string result = LLMEngine::RequestLogger::redactText(input);

    assert(result.find("sk_abcdefghijklmnopqrstuvwxyz012345") == std::string::npos);
    assert(result.find("<REDACTED>") != std::string::npos);

    std::cout << "✓ Quoted values redaction test passed\n";
}

int main() {
    std::cout << "=== Secret Redaction Tests ===\n";

    testRedactTextBasic();
    testRedactTextMultipleSecrets();
    testRedactTextColonFormat();
    testRedactUrl();
    testRedactHeaders();
    testLogSafe();
    testLogSafeNullLogger();
    testRedactTextNoSecrets();
    testRedactTextQuotedValues();

    std::cout << "\nAll secret redaction tests passed!\n";
    return 0;
}
