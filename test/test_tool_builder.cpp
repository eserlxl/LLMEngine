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

int main() {
    try {
        test_simple_function();
        std::cout << "All ToolBuilder tests passed." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
