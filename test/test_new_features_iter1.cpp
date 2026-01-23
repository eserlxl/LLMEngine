#include "../support/FakeAPIClient.hpp" // Relative path to support
#include "LLMEngine/APIClient.hpp"      // For RequestOptions
#include "LLMEngine/AnalysisInput.hpp"
#include "LLMEngine/AnalysisResult.hpp"
#include "LLMEngine/LLMEngine.hpp"

#include <cassert>
#include <future>
#include <iostream>
#include <thread>
#include <vector>

using namespace LLMEngine;
using namespace LLMEngineAPI;

void test_analysis_input_messages() {
    std::cout << "Running test_analysis_input_messages..." << std::endl;

    auto input = AnalysisInput::builder()
                     .withMessage("system", "You are a helper.")
                     .withMessage("user", "Hello");

    nlohmann::json json = input.toJson();

    assert(json.contains("messages"));
    assert(json["messages"].is_array());
    assert(json["messages"].size() == 2);
    assert(json["messages"][0]["role"] == "system");
    assert(json["messages"][0]["content"] == "You are a helper.");
    assert(json["messages"][1]["role"] == "user");
    assert(json["messages"][1]["content"] == "Hello");

    std::cout << "PASS" << std::endl;
}

void test_tool_call_extraction() {
    std::cout << "Running test_tool_call_extraction..." << std::endl;

    // Setup Fake Client
    auto clientPtr = std::make_unique<FakeAPIClient>();
    FakeAPIClient* fakeClient = clientPtr.get();

    LLMEngine engine(std::move(clientPtr), {}, 24, false, nullptr);

    // Setup Mock Response with Tool Calls
    APIResponse mockResponse;
    mockResponse.success = true;
    mockResponse.status_code = 200;
    mockResponse.content = ""; // usually empty for tool calls? or partial.
    mockResponse.finish_reason = "tool_calls";

    // Construct raw response mirroring OpenAI format
    mockResponse.raw_response = {
        {"choices",
         {{{"message",
            {{"content", nullptr},
             {"tool_calls",
              {{{"id", "call_123"},
                {"type", "function"},
                {"function",
                 {{"name", "get_weather"}, {"arguments", "{\"location\": \"Paris\"}"}}}}}}}},
           {"finish_reason", "tool_calls"}}}}};

    fakeClient->setNextResponse(mockResponse);

    AnalysisInput input;
    input.withUserMessage("What's the weather in Paris?");

    AnalysisResult result = engine.analyze(input, "test");

    assert(result.success);
    assert(result.hasToolCalls());
    assert(result.tool_calls.size() == 1);
    assert(result.tool_calls[0].id == "call_123");
    assert(result.tool_calls[0].name == "get_weather");
    assert(result.tool_calls[0].arguments == "{\"location\": \"Paris\"}");
    assert(result.finishReason == "tool_calls");

    std::cout << "PASS" << std::endl;
}

void test_async_options() {
    std::cout << "Running test_async_options..." << std::endl;

    auto clientPtr = std::make_unique<FakeAPIClient>();
    FakeAPIClient* fakeClient = clientPtr.get();
    LLMEngine engine(std::move(clientPtr), {}, 24, false, nullptr);

    RequestOptions options;
    options.timeout_ms = 999;
    options.max_retries = 3;

    AnalysisInput input;
    input.withUserMessage("hi");

    auto future = engine.analyzeAsync("hi", input, "test", options);
    auto result = future.get();

    assert(result.success);
    assert(fakeClient->last_options_.timeout_ms.has_value());
    assert(*fakeClient->last_options_.timeout_ms == 999);
    assert(*fakeClient->last_options_.max_retries == 3);

    std::cout << "PASS" << std::endl;
}

void test_stream_options() {
    std::cout << "Running test_stream_options..." << std::endl;

    auto clientPtr = std::make_unique<FakeAPIClient>();
    FakeAPIClient* fakeClient = clientPtr.get();
    LLMEngine engine(std::move(clientPtr), {}, 24, false, nullptr);

    RequestOptions options;
    options.timeout_ms = 888;

    AnalysisInput input;
    input.withUserMessage("hi");

    bool called = false;
    engine.analyzeStream("hi", input, "test", options, [&](std::string_view, bool done) {
        if (done)
            called = true;
    });

    assert(called);
    assert(fakeClient->last_options_.timeout_ms.has_value());
    assert(*fakeClient->last_options_.timeout_ms == 888);

    std::cout << "PASS" << std::endl;
}

int main() {
    try {
        test_analysis_input_messages();
        test_tool_call_extraction();
        test_async_options();
        test_stream_options();
        std::cout << "All integration tests passed." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
