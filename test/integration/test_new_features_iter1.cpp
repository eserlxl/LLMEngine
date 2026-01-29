#include "LLMEngine/core/AnalysisInput.hpp"
#include "LLMEngine/http/RequestOptions.hpp"
#include <cassert>
#include <iostream>

using namespace LLMEngine;

void testResponseFormatBuilder() {
    auto text = ResponseFormatBuilder::text();
    assert(text["type"] == "text");

    auto jsonObj = ResponseFormatBuilder::json_object();
    assert(jsonObj["type"] == "json_object");

    nlohmann::json schema = {
        {"type", "object"},
        {"properties", {{"foo", {{"type", "string"}}}}}};
    
    auto jsonSchema = ResponseFormatBuilder::json_schema("test_schema", schema, true);
    assert(jsonSchema["type"] == "json_schema");
    assert(jsonSchema["json_schema"]["name"] == "test_schema");
    assert(jsonSchema["json_schema"]["strict"] == true);
    
    std::cout << "testResponseFormatBuilder PASSED\n";
}

void testRequestOptionsBuilderExtensions() {
    auto options = RequestOptionsBuilder()
        .setReasoningEffort("high")
        .setMaxCompletionTokens(1024)
        .build();
    
    assert(options.generation.reasoning_effort.has_value());
    assert(*options.generation.reasoning_effort == "high");
    
    assert(options.generation.max_completion_tokens.has_value());
    assert(*options.generation.max_completion_tokens == 1024);
    
    std::cout << "testRequestOptionsBuilderExtensions PASSED\n";
}

void testAnalysisInputDeveloperMessage() {
    auto input = AnalysisInput::builder()
        .withDeveloperMessage("You are a dev tool.");
    
    assert(!input.messages.empty());
    assert(input.messages.back().role == "developer");
    assert(input.messages.back().getTextContent() == "You are a dev tool.");
    
    std::cout << "testAnalysisInputDeveloperMessage PASSED\n";
}

int main() {
    testResponseFormatBuilder();
    testRequestOptionsBuilderExtensions();
    testAnalysisInputDeveloperMessage();
    std::cout << "All Iter1Features tests passed.\n";
    return 0;
}
