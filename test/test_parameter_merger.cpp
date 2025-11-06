// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of the LLMEngine test suite (ParameterMerger tests) and is
// licensed under the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/ParameterMerger.hpp"
#include <cassert>
#include <iostream>
#include <nlohmann/json.hpp>

using namespace LLMEngine;

void testBasicMerge() {
    nlohmann::json base = {
        {"temperature", 0.7},
        {"max_tokens", 1000},
        {"top_p", 0.9}
    };
    
    nlohmann::json input = {
        {"temperature", 0.5}
    };
    
    auto result = ParameterMerger::merge(base, input, "");
    assert(result["temperature"] == 0.5);
    assert(result["max_tokens"] == 1000);
    assert(result["top_p"] == 0.9);
    std::cout << "✓ Basic merge test passed\n";
}

void testNoChange() {
    nlohmann::json base = {
        {"temperature", 0.7},
        {"max_tokens", 1000}
    };
    
    nlohmann::json input = {};
    
    auto result = ParameterMerger::merge(base, input, "");
    // Should return base unchanged (same object reference behavior)
    assert(result["temperature"] == 0.7);
    assert(result["max_tokens"] == 1000);
    std::cout << "✓ No change test passed\n";
}

void testModeMerge() {
    nlohmann::json base = {
        {"temperature", 0.7}
    };
    
    nlohmann::json input = {};
    
    auto result = ParameterMerger::merge(base, input, "test_mode");
    assert(result["mode"] == "test_mode");
    assert(result["temperature"] == 0.7);
    std::cout << "✓ Mode merge test passed\n";
}

void testTypeValidation() {
    nlohmann::json base = {
        {"temperature", 0.7},
        {"max_tokens", 1000}
    };
    
    // Invalid type should be ignored
    nlohmann::json input = {
        {"temperature", "invalid"}  // Should be ignored
    };
    
    auto result = ParameterMerger::merge(base, input, "");
    assert(result["temperature"] == 0.7);  // Should remain unchanged
    std::cout << "✓ Type validation test passed\n";
}

void testIntegerToFloatConversion() {
    nlohmann::json base = {
        {"temperature", 0.7}
    };
    
    // Integer should be accepted for float parameter
    nlohmann::json input = {
        {"temperature", 1}  // Integer, should be converted to float
    };
    
    auto result = ParameterMerger::merge(base, input, "");
    assert(result["temperature"] == 1.0);
    assert(result["temperature"].is_number_float());
    std::cout << "✓ Integer to float conversion test passed\n";
}

void testAllAllowedKeys() {
    nlohmann::json base = {
        {"temperature", 0.7},
        {"max_tokens", 1000}
    };
    
    nlohmann::json input = {
        {"temperature", 0.5},
        {"max_tokens", 500},
        {"top_p", 0.8},
        {"top_k", 40},
        {"min_p", 0.1},
        {"presence_penalty", 0.2},
        {"frequency_penalty", 0.3},
        {"timeout_seconds", 60}
    };
    
    auto result = ParameterMerger::merge(base, input, "");
    assert(result["temperature"] == 0.5);
    assert(result["max_tokens"] == 500);
    assert(result["top_p"] == 0.8);
    assert(result["top_k"] == 40);
    assert(result["min_p"] == 0.1);
    assert(result["presence_penalty"] == 0.2);
    assert(result["frequency_penalty"] == 0.3);
    assert(result["timeout_seconds"] == 60);
    std::cout << "✓ All allowed keys test passed\n";
}

void testUnallowedKeys() {
    nlohmann::json base = {
        {"temperature", 0.7}
    };
    
    nlohmann::json input = {
        {"invalid_key", "value"},  // Should be ignored
        {"another_invalid", 123}   // Should be ignored
    };
    
    auto result = ParameterMerger::merge(base, input, "");
    assert(result.contains("temperature"));
    assert(!result.contains("invalid_key"));
    assert(!result.contains("another_invalid"));
    std::cout << "✓ Unallowed keys test passed\n";
}

void testMergeInto() {
    nlohmann::json base = {
        {"temperature", 0.7}
    };
    
    nlohmann::json input = {
        {"temperature", 0.5}
    };
    
    nlohmann::json out;
    bool changed = ParameterMerger::mergeInto(base, input, "", out);
    assert(changed == true);
    assert(out["temperature"] == 0.5);
    
    // Test no change
    nlohmann::json empty_input = {};
    changed = ParameterMerger::mergeInto(base, empty_input, "", out);
    assert(changed == false);
    std::cout << "✓ MergeInto test passed\n";
}

int main() {
    std::cout << "=== ParameterMerger Unit Tests ===\n";
    
    testBasicMerge();
    testNoChange();
    testModeMerge();
    testTypeValidation();
    testIntegerToFloatConversion();
    testAllAllowedKeys();
    testUnallowedKeys();
    testMergeInto();
    
    std::cout << "\nAll ParameterMerger tests passed!\n";
    return 0;
}

