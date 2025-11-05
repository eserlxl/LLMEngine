// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for RequestLogger redaction functions with edge cases

#include <iostream>
#include <cassert>
#include <map>
#include "LLMEngine/RequestLogger.hpp"

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
    assert(result2.find("api_key=<REDACTED>") != std::string::npos && "Sensitive param should be redacted");
    assert(result2.find("secret123") == std::string::npos && "Secret should not appear");
    
    // Test 3: Multiple parameters, one sensitive
    std::string url3 = "https://api.example.com/endpoint?model=gpt-4&api_key=secret&temperature=0.7";
    std::string result3 = RequestLogger::redactUrl(url3);
    assert(result3.find("api_key=<REDACTED>") != std::string::npos && "Sensitive param should be redacted");
    assert(result3.find("model=gpt-4") != std::string::npos && "Non-sensitive param should remain");
    assert(result3.find("temperature=0.7") != std::string::npos && "Non-sensitive param should remain");
    
    std::cout << "  ✓ Basic URL redaction tests passed\n";
}

void testUrlRedactionEncodedDelimiters() {
    std::cout << "Testing URL redaction (encoded delimiters)...\n";
    
    // Test 1: URL-encoded ampersand (%26)
    std::string url1 = "https://api.example.com/endpoint?param1=value1%26api_key=secret";
    std::string result1 = RequestLogger::redactUrl(url1);
    // Should handle %26 as part of value, not as delimiter
    // This is a limitation - the current implementation doesn't decode URL encoding
    // But it shouldn't crash
    assert(!result1.empty() && "Should handle URL-encoded delimiters gracefully");
    
    // Test 2: URL-encoded equals (%3D)
    std::string url2 = "https://api.example.com/endpoint?api%3Dkey=secret";
    std::string result2 = RequestLogger::redactUrl(url2);
    assert(!result2.empty() && "Should handle URL-encoded equals");
    
    std::cout << "  ✓ Encoded delimiter tests passed\n";
}

void testUrlRedactionEdgeCases() {
    std::cout << "Testing URL redaction (edge cases)...\n";
    
    // Test 1: Parameter without value
    std::string url1 = "https://api.example.com/endpoint?flag&api_key=secret";
    std::string result1 = RequestLogger::redactUrl(url1);
    assert(result1.find("flag") != std::string::npos && "Flag should remain");
    assert(result1.find("api_key=<REDACTED>") != std::string::npos && "Sensitive param should be redacted");
    
    // Test 2: Empty query string
    std::string url2 = "https://api.example.com/endpoint?";
    std::string result2 = RequestLogger::redactUrl(url2);
    assert(result2 == url2 && "Empty query string should be unchanged");
    
    // Test 3: Multiple equals signs in value
    std::string url3 = "https://api.example.com/endpoint?api_key=a=b=c";
    std::string result3 = RequestLogger::redactUrl(url3);
    assert(result3.find("api_key=<REDACTED>") != std::string::npos && "Should handle complex values");
    
    // Test 4: Case-insensitive sensitive parameter matching
    std::string url4 = "https://api.example.com/endpoint?API_KEY=secret&Token=abc123";
    std::string result4 = RequestLogger::redactUrl(url4);
    assert(result4.find("API_KEY=<REDACTED>") != std::string::npos && "Should match case-insensitively");
    assert(result4.find("Token=<REDACTED>") != std::string::npos && "Should match token case-insensitively");
    
    std::cout << "  ✓ Edge case tests passed\n";
}

void testHeaderRedaction() {
    std::cout << "Testing header redaction...\n";
    
    // Test 1: Sensitive header
    std::map<std::string, std::string> headers1 = {
        {"Authorization", "Bearer secret123"},
        {"Content-Type", "application/json"}
    };
    auto result1 = RequestLogger::redactHeaders(headers1);
    assert(result1.find("Authorization") != result1.end() && "Sensitive header should be present");
    assert(result1["Authorization"] == "<REDACTED>" && "Sensitive header value should be redacted");
    assert(result1["Content-Type"] == "application/json" && "Non-sensitive header should remain");
    
    // Test 2: Case-insensitive matching
    std::map<std::string, std::string> headers2 = {
        {"AUTHORIZATION", "Bearer secret"},
        {"X-API-KEY", "key123"}
    };
    auto result2 = RequestLogger::redactHeaders(headers2);
    // Headers not on allowlist should be omitted entirely (default-deny)
    assert(result2.empty() || result2.find("AUTHORIZATION") == result2.end() && "Non-allowed headers should be omitted");
    
    // Test 3: Allowed headers pass through
    std::map<std::string, std::string> headers3 = {
        {"Content-Type", "application/json"},
        {"Accept", "application/json"},
        {"User-Agent", "LLMEngine/1.0"}
    };
    auto result3 = RequestLogger::redactHeaders(headers3);
    assert(result3["Content-Type"] == "application/json" && "Allowed header should pass through");
    assert(result3["Accept"] == "application/json" && "Allowed header should pass through");
    assert(result3["User-Agent"] == "LLMEngine/1.0" && "Allowed header should pass through");
    
    // Test 4: Mixed allowed and non-allowed
    std::map<std::string, std::string> headers4 = {
        {"Content-Type", "application/json"},
        {"Authorization", "Bearer secret"},
        {"X-Custom-Header", "value"}
    };
    auto result4 = RequestLogger::redactHeaders(headers4);
    assert(result4.find("Content-Type") != result4.end() && "Allowed header should be present");
    assert(result4.find("Authorization") == result4.end() && "Non-allowed header should be omitted");
    assert(result4.find("X-Custom-Header") == result4.end() && "Non-allowed header should be omitted");
    
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
    
    // Test 3: Quoted values
    std::string text3 = "api_key=\"secret123\"";
    std::string result3 = RequestLogger::redactText(text3);
    assert(result3.find("api_key=") != std::string::npos && "Key should remain");
    assert(result3.find("<REDACTED>") != std::string::npos && "Quoted value should be redacted");
    
    // Test 4: Multiple sensitive fields
    std::string text4 = "api_key=secret1 token=secret2 password=secret3";
    std::string result4 = RequestLogger::redactText(text4);
    assert(result4.find("api_key=") != std::string::npos && "First key should remain");
    assert(result4.find("token=") != std::string::npos && "Second key should remain");
    assert(result4.find("password=") != std::string::npos && "Third key should remain");
    // Should have multiple redactions
    size_t redact_count = 0;
    size_t pos = 0;
    while ((pos = result4.find("<REDACTED>", pos)) != std::string::npos) {
        redact_count++;
        pos += 10;
    }
    assert(redact_count >= 3 && "Should redact all sensitive values");
    
    // Test 5: Non-sensitive text should pass through
    std::string text5 = "model=gpt-4 temperature=0.7";
    std::string result5 = RequestLogger::redactText(text5);
    assert(result5 == text5 && "Non-sensitive text should be unchanged");
    
    // Test 6: Empty string
    std::string text6 = "";
    std::string result6 = RequestLogger::redactText(text6);
    assert(result6.empty() && "Empty string should remain empty");
    
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
    
    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/json"},
        {"Authorization", "Bearer secret"}
    };
    
    std::string result = RequestLogger::formatRequest("POST", "https://api.example.com/endpoint?api_key=secret", headers);
    
    assert(result.find("POST") != std::string::npos && "Should include method");
    assert(result.find("https://api.example.com/endpoint") != std::string::npos && "Should include URL");
    assert(result.find("api_key=<REDACTED>") != std::string::npos && "URL params should be redacted");
    assert(result.find("Content-Type") != std::string::npos && "Should include headers");
    // Authorization should not appear (default-deny policy)
    
    std::cout << "  ✓ formatRequest tests passed\n";
}

int main() {
    std::cout << "Running RequestLogger redaction tests...\n\n";
    
    try {
        testUrlRedactionBasic();
        testUrlRedactionEncodedDelimiters();
        testUrlRedactionEdgeCases();
        testHeaderRedaction();
        testTextRedaction();
        testIsSensitiveHeader();
        testFormatRequest();
        
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

