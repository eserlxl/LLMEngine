// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fuzz test target for JSON parsing operations.
// Build with: clang++ -fsanitize=fuzzer,address,undefined -O1 fuzz_json_parsing.cpp -o
// fuzz_json_parsing Or use CMake with ENABLE_FUZZ=ON (see test/CMakeLists.txt)

#include <cstddef>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0 || size > 100000) { // Reasonable size limit
        return 0;
    }

    // Create a string from the fuzzer input
    std::string input(reinterpret_cast<const char*>(data), size);

    // Test 1: Basic JSON parsing
    try {
        auto json = nlohmann::json::parse(input);
        // Try to access common fields to exercise the parser
        (void)json.is_object();
        (void)json.is_array();
        (void)json.is_string();
        (void)json.is_number();
        (void)json.is_boolean();
        (void)json.is_null();
    } catch (const nlohmann::json::exception&) {
        // Expected for malformed JSON - this is fine
    } catch (...) {
        // Other exceptions should not occur, but we catch them to prevent crashes
    }

    // Test 2: JSON merge operations (common in ParameterMerger)
    try {
        nlohmann::json base = {{"temperature", 0.7}, {"max_tokens", 1000}, {"top_p", 0.9}};

        // Try to parse input as JSON and merge
        try {
            auto override_json = nlohmann::json::parse(input);
            if (override_json.is_object()) {
                nlohmann::json result = base;
                result.update(override_json);
                // Access some fields to ensure merge worked
                (void)result.contains("temperature");
                (void)result.contains("max_tokens");
            }
        } catch (const nlohmann::json::exception&) {
            // Malformed JSON is expected
        }
    } catch (...) {
        // Ignore all exceptions
    }

    // Test 3: JSON array access
    try {
        auto json = nlohmann::json::parse(input);
        if (json.is_array() && !json.empty()) {
            // Try to access first element
            (void)json[0];
            // Try to access size
            (void)json.size();
        }
    } catch (const nlohmann::json::exception&) {
        // Expected for malformed JSON
    } catch (...) {
        // Ignore other exceptions
    }

    // Test 4: JSON object field access
    try {
        auto json = nlohmann::json::parse(input);
        if (json.is_object()) {
            // Try to access common fields
            (void)json.contains("choices");
            (void)json.contains("message");
            (void)json.contains("content");
            (void)json.contains("temperature");
            (void)json.contains("max_tokens");

            // Try to get values
            if (json.contains("choices")) {
                (void)json["choices"];
            }
        }
    } catch (const nlohmann::json::exception&) {
        // Expected for malformed JSON
    } catch (...) {
        // Ignore other exceptions
    }

    return 0;
}
