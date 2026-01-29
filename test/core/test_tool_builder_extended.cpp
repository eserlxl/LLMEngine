#include "LLMEngine/core/ToolBuilder.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Manual runner macros
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

#define ASSERT_FALSE(cond)                                                                         \
    if ((cond)) {                                                                                  \
        std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << #cond        \
                  << " is true" << std::endl;                                                      \
        throw std::runtime_error("Assertion failed");                                              \
    }


#define ASSERT_FALSE(cond)                                                                         \
    if ((cond)) {                                                                                  \
        std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << #cond        \
                  << " is true" << std::endl;                                                      \
        throw std::runtime_error("Assertion failed");                                              \
    }


using namespace LLMEngine;

void testNestedObject() {
    std::cout << "Testing Nested Object Tool..." << std::endl;
    
    // Address schema
    auto addressBytes = ToolBuilder::createSchema("Address object")
        .addStringProperty("street", "Street name", true)
        .addStringProperty("city", "City", true)
        .addStringProperty("zip", "Zip code");
        
    // Person schema
    auto personTool = ToolBuilder::createFunction("create_person", "Create a person record")
        .addStringProperty("name", "Full name", true)
        .addIntegerProperty("age", "Age in years")
        .addObjectProperty("address", addressBytes, "Home address", true);
        
    nlohmann::json tool = personTool.build();
    
    // std::cout << tool.dump(2) << std::endl;
    
    ASSERT_EQ(tool["type"], "function");
    ASSERT_EQ(tool["function"]["name"], "create_person");
    ASSERT_EQ(tool["function"]["parameters"]["type"], "object");
    
    auto props = tool["function"]["parameters"]["properties"];
    ASSERT_TRUE(props.contains("name"));
    ASSERT_TRUE(props.contains("address"));
    ASSERT_EQ(props["address"]["type"], "object");
    ASSERT_TRUE(props["address"]["properties"].contains("street"));
    
    std::cout << "PASS" << std::endl;
}

void testStrictMode() {
    std::cout << "Testing Strict Mode..." << std::endl;
    auto tool = ToolBuilder::createFunction("strict_func", "Strict function")
        .addStringProperty("param", "A param", true)
        .setStrict(true)
        .build();
        
    ASSERT_TRUE(tool["function"]["strict"].get<bool>());
    ASSERT_FALSE(tool["function"]["parameters"]["additionalProperties"].get<bool>());
    std::cout << "PASS" << std::endl;
}

void testArrayProperty() {
    std::cout << "Testing Array Property..." << std::endl;
    nlohmann::json itemSchema = {{"type", "string"}};
    
    auto tool = ToolBuilder::createFunction("process_tags", "Process a list of tags")
        .addArrayProperty("tags", itemSchema, "List of tags", true)
        .build();
        
    auto props = tool["function"]["parameters"]["properties"];
    ASSERT_EQ(props["tags"]["type"], "array");
    ASSERT_EQ(props["tags"]["items"]["type"], "string");
    std::cout << "PASS" << std::endl;
}

int main() {
    try {
        testNestedObject();
        testStrictMode();
        testArrayProperty();
        std::cout << "All extended tool tests passed." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Tests failed: " << e.what() << std::endl;
        return 1;
    }
}
