// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fuzz test target for response parsing operations.
// Build with: clang++ -fsanitize=fuzzer,address,undefined -O1 fuzz_response_parsing.cpp -o
// fuzz_response_parsing Or use CMake with ENABLE_FUZZ=ON (see test/CMakeLists.txt)

#include "LLMEngine/ResponseParser.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0 || size > 100000) { // Reasonable size limit
        return 0;
    }

    // Create a string from the fuzzer input
    std::string input(reinterpret_cast<const char*>(data), size);

    // Test ResponseParser::parseResponse with fuzzed input
    try {
        auto [think, remaining] = LLMEngine::ResponseParser::parseResponse(input);
        // Verify results are valid strings (no crashes)
        (void)think.size();
        (void)remaining.size();
        (void)think.empty();
        (void)remaining.empty();
    } catch (...) {
        // Ignore all exceptions - parseResponse should not throw
    }

    // Test with various think tag patterns
    try {
        // Test with think tags
        std::string with_think = "<think>" + input + "</think>More content";
        auto [think1, remaining1] = LLMEngine::ResponseParser::parseResponse(with_think);
        (void)think1.size();
        (void)remaining1.size();
    } catch (...) {
        // Ignore exceptions
    }

    // Test with nested think tags (edge case)
    try {
        std::string nested = "<think>Outer<think>" + input + "</think></think>Content";
        auto [think2, remaining2] = LLMEngine::ResponseParser::parseResponse(nested);
        (void)think2.size();
        (void)remaining2.size();
    } catch (...) {
        // Ignore exceptions
    }

    // Test with multiple think tags
    try {
        std::string multiple = "<think>First</think>" + input + "<think>Second</think>More";
        auto [think3, remaining3] = LLMEngine::ResponseParser::parseResponse(multiple);
        (void)think3.size();
        (void)remaining3.size();
    } catch (...) {
        // Ignore exceptions
    }

    // Test with unclosed think tags
    try {
        std::string unclosed = "<think>" + input;
        auto [think4, remaining4] = LLMEngine::ResponseParser::parseResponse(unclosed);
        (void)think4.size();
        (void)remaining4.size();
    } catch (...) {
        // Ignore exceptions
    }

    // Test with mismatched tags
    try {
        std::string mismatched = "<think>" + input + "</think2>";
        auto [think5, remaining5] = LLMEngine::ResponseParser::parseResponse(mismatched);
        (void)think5.size();
        (void)remaining5.size();
    } catch (...) {
        // Ignore exceptions
    }

    return 0;
}
