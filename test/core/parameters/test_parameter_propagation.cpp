#include "LLMEngine/core/LLMEngine.hpp"
#include "LLMEngine/core/ParameterMerger.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>
#include <string>

// Manual Assertion Macros to match existing suite style
#define ASSERT_TRUE(cond)                                                                          \
    if (!(cond)) {                                                                                 \
        std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << #cond        \
                  << std::endl;                                                                    \
        throw std::runtime_error("Assertion failed");                                              \
    }

#define ASSERT_EQ(a, b)                                                                            \
    if ((a) != (b)) {                                                                              \
        std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << #a           \
                  << " == " << #b << " (Values: " << (a) << " vs " << (b) << ")" << std::endl;     \
        throw std::runtime_error("Assertion failed");                                              \
    }

using namespace LLMEngine;

void testRequestOptionsMerge() {
    std::cout << "Testing ParameterMerger::mergeRequestOptions..." << std::endl;
    nlohmann::json params;
    RequestOptions options;
    RequestOptionsBuilder builder;
    builder.setTemperature(0.7)
           .setMaxTokens(100)
           .setTopP(0.9)
           .setFrequencyPenalty(0.5)
           .setResponseFormat({{"type", "json_object"}})
           .setToolChoice({{"type", "function"}, {"function", {{"name", "my_tool"}}}})
           .setUser("test-user");
           
    options = builder.build();
    
    // Test merging
    ParameterMerger::mergeRequestOptions(params, options);
    
    ASSERT_EQ(params["temperature"], 0.7);
    ASSERT_EQ(params["max_tokens"], 100);
    ASSERT_EQ(params["top_p"], 0.9);
    ASSERT_EQ(params["frequency_penalty"], 0.5);
    ASSERT_EQ(params["user"], "test-user");
    ASSERT_EQ(params["response_format"]["type"], "json_object");
    ASSERT_EQ(params["tool_choice"]["function"]["name"], "my_tool");
    std::cout << "PASS" << std::endl;
}

void testJSONOverridesAllowedKeys() {
    std::cout << "Testing ParameterMerger::mergeInto..." << std::endl;
    nlohmann::json base = {{"model", "gpt-4"}};
    nlohmann::json input = {
        {"temperature", 0.1},
        {"response_format", {{"type", "json_object"}}},
        {"stop", {"STOP"}},
        {"seed", 42},
        {"parallel_tool_calls", false}
    };
    
    nlohmann::json merged;
    bool changed = ParameterMerger::mergeInto(base, input, "chat", merged, nullptr);
    
    ASSERT_TRUE(changed);
    ASSERT_EQ(merged["temperature"], 0.1);
    ASSERT_EQ(merged["response_format"]["type"], "json_object");
    ASSERT_EQ(merged["stop"][0], "STOP");
    ASSERT_EQ(merged["seed"], 42);
    ASSERT_EQ(merged["parallel_tool_calls"], false);
    std::cout << "PASS" << std::endl;
}

int main() {
    try {
        testRequestOptionsMerge();
        testJSONOverridesAllowedKeys();
        std::cout << "All parameter propagation tests passed." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Tests failed: " << e.what() << std::endl;
        return 1;
    }
}
