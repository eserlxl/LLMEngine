// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fuzz test target for parameter merging operations.
// Build with: clang++ -fsanitize=fuzzer,address,undefined -O1 fuzz_parameter_merging.cpp -o
// fuzz_parameter_merging Or use CMake with ENABLE_FUZZ=ON (see test/CMakeLists.txt)

#include "LLMEngine/core/ParameterMerger.hpp"

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

    // Base parameters (typical configuration)
    nlohmann::json base_params = {{"temperature", 0.7},
                                  {"max_tokens", 1000},
                                  {"top_p", 0.9},
                                  {"frequency_penalty", 0.0},
                                  {"presence_penalty", 0.0}};

    // Test 1: Parse input as JSON and merge
    try {
        nlohmann::json input_json;
        try {
            input_json = nlohmann::json::parse(input);
        } catch (const nlohmann::json::exception&) {
            // Malformed JSON is expected - return early
            return 0;
        }

        // Test merge with empty mode
        try {
            auto result = LLMEngine::ParameterMerger::merge(base_params, input_json, "");
            // Verify result is valid JSON
            (void)result.is_object();
            (void)result.contains("temperature");
        } catch (...) {
            // Ignore exceptions from merge
        }

        // Test merge with fuzzed mode string
        try {
            std::string mode = input.substr(0, std::min(size, size_t(100)));
            auto result = LLMEngine::ParameterMerger::merge(base_params, input_json, mode);
            (void)result.is_object();
        } catch (...) {
            // Ignore exceptions
        }

        // Test mergeInto
        try {
            nlohmann::json out;
            bool changed = LLMEngine::ParameterMerger::mergeInto(base_params, input_json, "", out);
            (void)changed;
            (void)out.is_object();
        } catch (...) {
            // Ignore exceptions
        }

    } catch (...) {
        // Ignore all exceptions
    }

    // Test 2: Merge with various parameter types
    try {
        nlohmann::json test_input;

        // Try to create a valid JSON object from input
        if (size > 10) {
            // Create a simple JSON object with fuzzed values
            test_input["temperature"] = static_cast<double>(data[0] % 200) / 100.0; // 0.0-1.99
            test_input["max_tokens"] = static_cast<int>(data[1] % 10000);

            if (size > 20) {
                test_input["top_p"] = static_cast<double>(data[2] % 200) / 100.0;
            }

            // Try merge
            auto result = LLMEngine::ParameterMerger::merge(base_params, test_input, "");
            (void)result.is_object();
        }
    } catch (...) {
        // Ignore exceptions
    }

    // Test 3: Merge with invalid types (should be ignored)
    try {
        nlohmann::json invalid_input = {
            {"temperature", input}, // String instead of number
            {"max_tokens", input}   // String instead of number
        };

        auto result = LLMEngine::ParameterMerger::merge(base_params, invalid_input, "");
        // Should still be valid, invalid types should be ignored
        (void)result.is_object();
    } catch (...) {
        // Ignore exceptions
    }

    // Test 4: Merge with unallowed keys (should be ignored)
    try {
        nlohmann::json unallowed_input;
        if (size > 5) {
            std::string key = input.substr(0, std::min(size, size_t(50)));
            unallowed_input[key] = "value";

            auto result = LLMEngine::ParameterMerger::merge(base_params, unallowed_input, "");
            (void)result.is_object();
        }
    } catch (...) {
        // Ignore exceptions
    }

    return 0;
}
