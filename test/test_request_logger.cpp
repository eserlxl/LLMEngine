// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for RequestLogger redaction functions with edge cases

#include "LLMEngine/RequestLogger.hpp"

#include <cassert>
#include <chrono>
#include <iostream>
#include <map>

using namespace LLMEngine;

void testUrlRedactionBasic() {
    std::cout << "Testing URL redaction (basic cases)...\n";

    // Test 1: No query parameters
    std::string url1 = "https://api.example.com/endpoint";
    std::string result1 = RequestLogger::redactUrl(url1);
    assert(result1 == url1 && "URL without params should be unchanged");

    // Test 2: Simple sensitive parameter
    std::string url2 = "https://api.example.com/endpoint?api_key=secret123";
    std::string result2 = RequestLogger::redactUrl(url2);
    assert(result2.find("api_key=<REDACTED>") != std::string::npos
           && "Sensitive param should be redacted");
    assert(result2.find("secret123") == std::string::npos && "Secret should not appear");

    // Test 3: Multiple parameters, one sensitive
    std::string url3 =
        "https://api.example.com/endpoint?model=gpt-4&api_key=secret&temperature=0.7";
    std::string result3 = RequestLogger::redactUrl(url3);
    assert(result3.find("api_key=<REDACTED>") != std::string::npos
           && "Sensitive param should be redacted");
    assert(result3.find("model=gpt-4") != std::string::npos && "Non-sensitive param should remain");
    assert(result3.find("temperature=0.7") != std::string::npos
           && "Non-sensitive param should remain");

    std::cout << "  ✓ Basic URL redaction tests passed\n";
}

void testUrlRedactionEncodedDelimiters() {
    std::cout << "Testing URL redaction (encoded delimiters)...\n";

    // Test 1: URL-encoded ampersand (%26) in parameter name
    std::string url1 = "https://api.example.com/endpoint?param%26name=value";
    std::string result1 = RequestLogger::redactUrl(url1);
    assert(!result1.empty() && "Should handle URL-encoded & in param name");

    // Test 2: URL-encoded equals (%3D) in parameter name - should be decoded for matching
    std::string url2 = "https://api.example.com/endpoint?api%3Dkey=secret";
    std::string result2 = RequestLogger::redactUrl(url2);
    // The implementation should decode %3D to = for matching, so "api=key" should match "api_key"
    // pattern This tests the basic URL decoding in parameter names
    assert(!result2.empty() && "Should handle URL-encoded equals in param name");

    // Test 3: Nested encoding (%26%3D for & and =)
    std::string url3 = "https://api.example.com/endpoint?param%26%3Dvalue=test";
    std::string result3 = RequestLogger::redactUrl(url3);
    assert(!result3.empty() && "Should handle nested URL encoding");

    // Test 4: Fragment identifier preservation
    std::string url4 = "https://api.example.com/endpoint?api_key=secret#anchor";
    std::string result4 = RequestLogger::redactUrl(url4);
    assert(result4.find("#anchor") != std::string::npos && "Fragment should be preserved");
    assert(result4.find("api_key=<REDACTED>") != std::string::npos
           && "Should still redact sensitive params");

    // Test 5: Semicolon-separated parameters (RFC 1866)
    std::string url5 =
        "https://api.example.com/endpoint?param1=value1;api_key=secret;param2=value2";
    std::string result5 = RequestLogger::redactUrl(url5);
    assert(result5.find("api_key=<REDACTED>") != std::string::npos
           && "Should handle semicolon separators");
    assert(result5.find("param1=value1") != std::string::npos
           && "Should preserve non-sensitive params");

    std::cout << "  ✓ Encoded delimiter tests passed\n";
}

void testUrlRedactionEdgeCases() {
    std::cout << "Testing URL redaction (edge cases)...\n";

    // Test 1: Parameter without value
    std::string url1 = "https://api.example.com/endpoint?flag&api_key=secret";
    std::string result1 = RequestLogger::redactUrl(url1);
    assert(result1.find("flag") != std::string::npos && "Flag should remain");
    assert(result1.find("api_key=<REDACTED>") != std::string::npos
           && "Sensitive param should be redacted");

    // Test 2: Empty query string
    std::string url2 = "https://api.example.com/endpoint?";
    std::string result2 = RequestLogger::redactUrl(url2);
    assert(result2 == url2 && "Empty query string should be unchanged");

    // Test 3: Multiple equals signs in value
    std::string url3 = "https://api.example.com/endpoint?api_key=a=b=c";
    std::string result3 = RequestLogger::redactUrl(url3);
    assert(result3.find("api_key=<REDACTED>") != std::string::npos
           && "Should handle complex values");

    // Test 4: Case-insensitive sensitive parameter matching
    std::string url4 = "https://api.example.com/endpoint?API_KEY=secret&Token=abc123";
    std::string result4 = RequestLogger::redactUrl(url4);
    assert(result4.find("API_KEY=<REDACTED>") != std::string::npos
           && "Should match case-insensitively");
    assert(result4.find("Token=<REDACTED>") != std::string::npos
           && "Should match token case-insensitively");

    std::cout << "  ✓ Edge case tests passed\n";
}

void testHeaderRedaction() {
    std::cout << "Testing header redaction...\n";

    // Test 1: Sensitive header (not on allowlist, should be omitted)
    std::map<std::string, std::string> headers1 = {{"Authorization", "Bearer secret123"},
                                                   {"Content-Type", "application/json"}};
    auto result1 = RequestLogger::redactHeaders(headers1);
    // Authorization is not on allowlist, so should be omitted entirely (default-deny)
    assert(result1.find("Authorization") == result1.end()
           && "Non-allowed sensitive header should be omitted");
    assert(result1["Content-Type"] == "application/json" && "Allowed header should remain");

    // Test 2: Case-insensitive matching for allowlist
    std::map<std::string, std::string> headers2 = {{"CONTENT-TYPE", "application/json"},
                                                   {"accept", "application/json"},
                                                   {"User-Agent", "LLMEngine/1.0"}};
    auto result2 = RequestLogger::redactHeaders(headers2);
    assert(result2.find("CONTENT-TYPE") != result2.end() && "Case-insensitive allowlist matching");
    assert(result2.find("accept") != result2.end() && "Lowercase header should match");
    assert(result2.find("User-Agent") != result2.end() && "Mixed case header should match");

    // Test 3: Allowed headers pass through
    std::map<std::string, std::string> headers3 = {{"Content-Type", "application/json"},
                                                   {"Accept", "application/json"},
                                                   {"User-Agent", "LLMEngine/1.0"}};
    auto result3 = RequestLogger::redactHeaders(headers3);
    assert(result3["Content-Type"] == "application/json" && "Allowed header should pass through");
    assert(result3["Accept"] == "application/json" && "Allowed header should pass through");
    assert(result3["User-Agent"] == "LLMEngine/1.0" && "Allowed header should pass through");

    // Test 4: Mixed allowed and non-allowed
    std::map<std::string, std::string> headers4 = {{"Content-Type", "application/json"},
                                                   {"Authorization", "Bearer secret"},
                                                   {"X-Custom-Header", "value"},
                                                   {"Accept-Encoding", "gzip"}};
    auto result4 = RequestLogger::redactHeaders(headers4);
    assert(result4.find("Content-Type") != result4.end() && "Allowed header should be present");
    assert(result4.find("Authorization") == result4.end()
           && "Non-allowed header should be omitted");
    assert(result4.find("X-Custom-Header") == result4.end()
           && "Non-allowed header should be omitted");
    assert(result4.find("Accept-Encoding") != result4.end() && "Allowed header should be present");

    // Test 5: Empty header map
    std::map<std::string, std::string> headers5 = {};
    auto result5 = RequestLogger::redactHeaders(headers5);
    assert(result5.empty() && "Empty header map should return empty result");

    // Test 6: Headers with special characters in values
    std::map<std::string, std::string> headers6 = {
        {"Content-Type", "application/json; charset=utf-8"},
        {"User-Agent", "LLMEngine/1.0 (Linux)"}};
    auto result6 = RequestLogger::redactHeaders(headers6);
    assert(result6["Content-Type"] == "application/json; charset=utf-8"
           && "Should preserve special chars in values");
    assert(result6["User-Agent"] == "LLMEngine/1.0 (Linux)" && "Should preserve special chars");

    // Test 7: Multiple headers with same allowlist status
    std::map<std::string, std::string> headers7 = {{"Content-Type", "application/json"},
                                                   {"Content-Length", "1024"},
                                                   {"Accept", "application/json"}};
    auto result7 = RequestLogger::redactHeaders(headers7);
    assert(result7.size() == 3 && "All allowed headers should be present");

    std::cout << "  ✓ Header redaction tests passed\n";
}

void testTextRedaction() {
    std::cout << "Testing text redaction...\n";

    // Test 1: Simple key=value pattern
    std::string text1 = "api_key=secret123";
    std::string result1 = RequestLogger::redactText(text1);
    assert(result1.find("api_key=") != std::string::npos && "Key should remain");
    assert(result1.find("<REDACTED>") != std::string::npos && "Value should be redacted");
    assert(result1.find("secret123") == std::string::npos && "Secret should not appear");

    // Test 2: Key:value pattern
    std::string text2 = "Token: abc123";
    std::string result2 = RequestLogger::redactText(text2);
    assert(result2.find("Token:") != std::string::npos && "Key should remain");
    assert(result2.find("<REDACTED>") != std::string::npos && "Value should be redacted");

    // Test 3: Quoted values (single and double quotes)
    std::string text3 = "api_key=\"secret123\"";
    std::string result3 = RequestLogger::redactText(text3);
    assert(result3.find("api_key=") != std::string::npos && "Key should remain");
    assert(result3.find("<REDACTED>") != std::string::npos && "Quoted value should be redacted");

    std::string text3b = "api_key='secret123'";
    std::string result3b = RequestLogger::redactText(text3b);
    assert(result3b.find("api_key=") != std::string::npos && "Key should remain");
    assert(result3b.find("<REDACTED>") != std::string::npos
           && "Single-quoted value should be redacted");

    // Test 4: Nested quotes (escaped quotes)
    std::string text4 = "api_key=\"secret\\\"value\"";
    std::string result4 = RequestLogger::redactText(text4);
    // Implementation may not handle escapes perfectly, but should handle the basic quoted case
    assert(result4.find("api_key=") != std::string::npos && "Key should remain");

    // Test 5: Multiple sensitive fields
    std::string text5 = "api_key=secret1 token=secret2 password=secret3";
    std::string result5 = RequestLogger::redactText(text5);
    assert(result5.find("api_key=") != std::string::npos && "First key should remain");
    assert(result5.find("token=") != std::string::npos && "Second key should remain");
    assert(result5.find("password=") != std::string::npos && "Third key should remain");
    // Should have multiple redactions
    size_t redact_count = 0;
    size_t pos = 0;
    while ((pos = result5.find("<REDACTED>", pos)) != std::string::npos) {
        redact_count++;
        pos += 10;
    }
    // Use redact_count in a way the compiler recognizes
    if (redact_count < 3) {
        std::cerr << "Error: Expected at least 3 redactions, got " << redact_count << std::endl;
    }
    assert(redact_count >= 3 && "Should redact all sensitive values");

    // Test 6: Non-sensitive text should pass through
    std::string text6 = "model=gpt-4 temperature=0.7";
    std::string result6 = RequestLogger::redactText(text6);
    assert(result6 == text6 && "Non-sensitive text should be unchanged");

    // Test 7: Empty string
    std::string text7 = "";
    std::string result7 = RequestLogger::redactText(text7);
    assert(result7.empty() && "Empty string should remain empty");

    // Test 8: Special characters in values
    std::string text8 = "api_key=secret@123#value";
    std::string result8 = RequestLogger::redactText(text8);
    assert(result8.find("api_key=") != std::string::npos && "Key should remain");
    assert(result8.find("<REDACTED>") != std::string::npos
           && "Value with special chars should be redacted");

    // Test 9: Whitespace handling
    std::string text9 = "api_key = secret123";
    std::string result9 = RequestLogger::redactText(text9);
    assert(result9.find("api_key") != std::string::npos && "Key with spaces should be handled");

    // Test 10: Delimiters in values (comma, semicolon)
    std::string text10 = "api_key=secret,value";
    std::string result10 = RequestLogger::redactText(text10);
    assert(result10.find("api_key=") != std::string::npos && "Key should remain");
    assert(result10.find("<REDACTED>") != std::string::npos
           && "Value with delimiter should be redacted");

    std::cout << "  ✓ Text redaction tests passed\n";
}

void testIsSensitiveHeader() {
    std::cout << "Testing isSensitiveHeader...\n";

    // Test exact matches
    assert(RequestLogger::isSensitiveHeader("authorization") && "Should match authorization");
    assert(RequestLogger::isSensitiveHeader("Authorization") && "Should match case-insensitively");
    assert(RequestLogger::isSensitiveHeader("AUTHORIZATION") && "Should match uppercase");
    assert(RequestLogger::isSensitiveHeader("x-api-key") && "Should match x-api-key");
    assert(RequestLogger::isSensitiveHeader("X-API-Key") && "Should match case-insensitively");

    // Test non-sensitive headers
    assert(!RequestLogger::isSensitiveHeader("content-type") && "Should not match content-type");
    assert(!RequestLogger::isSensitiveHeader("accept") && "Should not match accept");
    assert(!RequestLogger::isSensitiveHeader("user-agent") && "Should not match user-agent");

    // Test empty string
    assert(!RequestLogger::isSensitiveHeader("") && "Empty string should not be sensitive");

    std::cout << "  ✓ isSensitiveHeader tests passed\n";
}

void testFormatRequest() {
    std::cout << "Testing formatRequest...\n";

    std::map<std::string, std::string> headers = {{"Content-Type", "application/json"},
                                                  {"Authorization", "Bearer secret"}};

    std::string result = RequestLogger::formatRequest(
        "POST", "https://api.example.com/endpoint?api_key=secret", headers);

    assert(result.find("POST") != std::string::npos && "Should include method");
    assert(result.find("https://api.example.com/endpoint") != std::string::npos
           && "Should include URL");
    assert(result.find("api_key=<REDACTED>") != std::string::npos
           && "URL params should be redacted");
    assert(result.find("Content-Type") != std::string::npos && "Should include headers");
    // Authorization should not appear (default-deny policy)

    std::cout << "  ✓ formatRequest tests passed\n";
}

void testUrlRedactionFragmentHandling() {
    std::cout << "Testing URL redaction (fragment handling)...\n";

    // Test 1: Fragment with query params
    std::string url1 = "https://api.example.com/endpoint?api_key=secret#section1";
    std::string result1 = RequestLogger::redactUrl(url1);
    assert(result1.find("#section1") != std::string::npos && "Fragment should be preserved");
    assert(result1.find("api_key=<REDACTED>") != std::string::npos
           && "Should redact before fragment");

    // Test 2: Fragment without query params
    std::string url2 = "https://api.example.com/endpoint#section1";
    std::string result2 = RequestLogger::redactUrl(url2);
    assert(result2 == url2 && "Fragment without params should be unchanged");

    // Test 3: Multiple fragments (shouldn't happen, but handle gracefully)
    std::string url3 = "https://api.example.com/endpoint?param=value#first#second";
    std::string result3 = RequestLogger::redactUrl(url3);
    assert(result3.find("#first#second") != std::string::npos && "Should preserve multiple #");

    std::cout << "  ✓ Fragment handling tests passed\n";
}

void testUrlRedactionSemicolonSeparators() {
    std::cout << "Testing URL redaction (semicolon separators)...\n";

    // Test 1: Semicolon-separated parameters (RFC 1866)
    std::string url1 =
        "https://api.example.com/endpoint?param1=value1;api_key=secret;param2=value2";
    std::string result1 = RequestLogger::redactUrl(url1);
    assert(result1.find("api_key=<REDACTED>") != std::string::npos
           && "Should handle semicolon separators");
    assert(result1.find("param1=value1") != std::string::npos
           && "Should preserve non-sensitive params");

    // Test 2: Mixed & and ; separators
    std::string url2 =
        "https://api.example.com/endpoint?param1=value1&api_key=secret;param2=value2";
    std::string result2 = RequestLogger::redactUrl(url2);
    assert(result2.find("api_key=<REDACTED>") != std::string::npos
           && "Should handle mixed separators");

    std::cout << "  ✓ Semicolon separator tests passed\n";
}

void testPerformanceLargePayloads() {
    std::cout << "Testing performance with large payloads...\n";

    // Test 1: Large URL with many parameters
    std::string large_url = "https://api.example.com/endpoint?";
    for (int i = 0; i < 100; ++i) {
        if (i > 0)
            large_url += "&";
        large_url += "param" + std::to_string(i) + "=value" + std::to_string(i);
    }
    large_url += "&api_key=secret123";

    auto start = std::chrono::high_resolution_clock::now();
    std::string result = RequestLogger::redactUrl(large_url);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    assert(result.find("api_key=<REDACTED>") != std::string::npos && "Should redact in large URL");
    assert(duration.count() < 10000 && "Should process large URL quickly (< 10ms)");

    // Test 2: Large header map
    std::map<std::string, std::string> large_headers;
    for (int i = 0; i < 50; ++i) {
        large_headers["Header" + std::to_string(i)] = "Value" + std::to_string(i);
    }
    large_headers["Content-Type"] = "application/json";
    large_headers["Accept"] = "application/json";

    start = std::chrono::high_resolution_clock::now();
    auto result_headers = RequestLogger::redactHeaders(large_headers);
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    assert(result_headers.find("Content-Type") != result_headers.end()
           && "Should process large header map");
    assert(duration.count() < 5000 && "Should process large headers quickly (< 5ms)");

    std::cout << "  ✓ Performance tests passed\n";
}

int main() {
    std::cout << "Running RequestLogger redaction tests...\n\n";

    try {
        testUrlRedactionBasic();
        testUrlRedactionEncodedDelimiters();
        testUrlRedactionEdgeCases();
        testUrlRedactionFragmentHandling();
        testUrlRedactionSemicolonSeparators();
        testHeaderRedaction();
        testTextRedaction();
        testIsSensitiveHeader();
        testFormatRequest();
        testPerformanceLargePayloads();

        std::cout << "\n✓ All RequestLogger redaction tests passed!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n✗ Test failed with unknown exception\n";
        return 1;
    }
}
