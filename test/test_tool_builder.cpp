// Copyright © 2026 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LLMEngine/ToolBuilder.hpp"
#include <cassert>
#include <iostream>

using namespace LLMEngine;

void test_simple_function() {
    std::cout << "Running test_simple_function..." << std::endl;
    auto tool = ToolBuilder::createFunction("get_current_weather", "Get the current weather in a given location")
                    .addStringProperty("location", "The city and state, e.g. San Francisco, CA", true)
                    .addEnumProperty("unit", {"celsius", "fahrenheit"}, "The temperature unit")
                    .build();

    assert(tool["type"] == "function");
    auto func = tool["function"];
    assert(func["name"] == "get_current_weather");
    assert(func.contains("parameters"));
    auto params = func["parameters"];
    assert(params["type"] == "object");
    assert(params["required"].size() == 1);
    assert(params["required"][0] == "location");
    assert(params["properties"]["location"]["type"] == "string");
    assert(params["properties"]["unit"]["enum"].size() == 2);
    
    std::cout << "PASS" << std::endl;
}

void test_strict_mode() {
    std::cout << "Running test_strict_mode..." << std::endl;
    auto tool = ToolBuilder::createFunction("strict_func", "Strict function")
                    .addStringProperty("prop1", "Property 1")
                    .addNumberProperty("prop2", "Property 2")
                    .setStrict(true)
                    .build();

    assert(tool["function"]["strict"] == true);
    auto params = tool["function"]["parameters"];
    assert(params["additionalProperties"] == false);
    
    // Check that all properties are automatically marked required
    auto req = params["required"];
    assert(req.size() == 2);
    
    // basic check for existence
    bool has_prop1 = false;
    bool has_prop2 = false;
    for (const auto& r : req) {
        if (r == "prop1") has_prop1 = true;
        if (r == "prop2") has_prop2 = true;
    }
    assert(has_prop1);
    assert(has_prop2);

    std::cout << "PASS" << std::endl;
}

int main() {
    try {
        test_simple_function();
        test_strict_mode();
        std::cout << "All ToolBuilder tests passed." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
