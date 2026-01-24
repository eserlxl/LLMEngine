// Copyright © 2026 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../src/ChatCompletionRequestHelper.hpp" // Access internal helper
#include "LLMEngine/Constants.hpp"
#include "LLMEngine/ErrorCodes.hpp"
#include "LLMEngine/ProviderBootstrap.hpp"
#include "LLMEngine/ResponseHandler.hpp"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace LLMEngine;
using namespace LLMEngineAPI;

void test_provider_bootstrap_env() {
    std::cout << "Testing ProviderBootstrap Env Overrides...\n";

    // Test OLLAMA_HOST
    setenv("OLLAMA_HOST", "http://env-host:11434", 1);
    std::string url = ProviderBootstrap::resolveBaseUrl(ProviderType::OLLAMA, "", "");
    assert(url == "http://env-host:11434");
    unsetenv("OLLAMA_HOST");

    // Test Model Override
    setenv("DEFAULT_MODEL", "env-model-v1", 1);
    std::string model = ProviderBootstrap::resolveModel(ProviderType::OPENAI, "", "");
    assert(model == "env-model-v1");
    unsetenv("DEFAULT_MODEL");

    // Test Provider Specific Model
    setenv("OPENAI_MODEL", "gpt-4-env", 1);
    model = ProviderBootstrap::resolveModel(ProviderType::OPENAI, "", "");
    assert(model == "gpt-4-env");
    unsetenv("OPENAI_MODEL");

    std::cout << "ProviderBootstrap tests passed.\n";
}

void test_chat_message_builder_images() {
    std::cout << "Testing ChatMessageBuilder Images...\n";

    nlohmann::json input;
    input["images"] = {"http://example.com/image.png"};

    // Simulate APIClient calling buildMessages
    // Using simple text prompt "Look at this"
    nlohmann::json messages = ChatMessageBuilder::buildMessages("Look at this", input);

    assert(messages.is_array());
    assert(messages.size() == 1); // 1 user message

    auto& msg = messages[0];
    assert(msg["role"] == "user");
    assert(msg["content"].is_array());

    auto& content = msg["content"];
    assert(content.size() == 2); // Text + 1 Image

    assert(content[0]["type"] == "text");
    assert(content[0]["text"] == "Look at this");

    assert(content[1]["type"] == "image_url");
    assert(content[1]["image_url"]["url"] == "http://example.com/image.png");

    std::cout << "ChatMessageBuilder tests passed.\n";
}

void test_response_handler_429() {
    std::cout << "Testing ResponseHandler 429...\n";

    LLMEngineAPI::APIResponse api_resp;
    api_resp.success = false;
    api_resp.status_code = 429;
    api_resp.error_message = "Too Many Requests";

    // Handle
    auto result = ResponseHandler::handle(api_resp, nullptr, "/tmp", "test", false, nullptr);

    assert(result.success == false);
    assert(result.statusCode == 429);
    assert(result.errorCode == LLMEngineErrorCode::RateLimited);

    std::cout << "ResponseHandler tests passed.\n";
}

#include "LLMEngine/ToolBuilder.hpp"
#include "LLMEngine/AnalysisInput.hpp"
#include "LLMEngine/RequestOptions.hpp"

void test_analysis_input_helpers() {
    std::cout << "Testing AnalysisInput Tool Helpers...\n";
    AnalysisInput input;
    
    // Test addTool
    auto tool = ToolBuilder::createFunction("weather", "get weather").build();
    // Cannot pass raw json to addTool? No, addTool takes ToolBuilder.
    // So I need ToolBuilder instance.
    auto builder = ToolBuilder::createFunction("weather", "get weather");
    input.addTool(builder);
    
    nlohmann::json json = input.toJson();
    assert(json.contains("tools"));
    assert(json["tools"].size() == 1);
    assert(json["tools"][0]["function"]["name"] == "weather");
    
    // Test addToolOutput
    input.addToolOutput("call_123", "sunny");
    json = input.toJson();
    // tools should still be there
    assert(json["tools"].size() == 1);
    
    // verify message added
    bool found = false;
    for (const auto& m : json["messages"]) {
        if (m["role"] == "tool" && m["tool_call_id"] == "call_123") {
            found = true;
            assert(m["content"] == "sunny");
        }
    }
    (void)found; // Suppress unused warning in Release
    assert(found);
    
    std::cout << "AnalysisInput Helpers passed.\n";
}

void test_param_propagation() {
    std::cout << "Testing ChatCompletionRequestHelper Param Propagation...\n";
    
    RequestOptions options;
    options.generation.seed = 12345;
    options.generation.service_tier = "auto";
    options.generation.parallel_tool_calls = true;
    options.stream_options = {true}; // include_usage
    
    nlohmann::json default_params;
    nlohmann::json params;
    
    bool verified = false;
    
    // Mock execution
    ChatCompletionRequestHelper::execute(
        default_params,
        params,
        // PayloadBuilder
        [&](const nlohmann::json& p) {

            // Verify params were merged from options
            assert(p.contains("seed"));
            assert(p["seed"] == 12345);
            assert(p.contains("service_tier"));
            assert(p["service_tier"] == "auto");
            assert(p.contains("parallel_tool_calls"));
            assert(p["parallel_tool_calls"] == true);
            assert(p.contains("stream_options"));
            assert(p["stream_options"]["include_usage"] == true);
            verified = true;
            return nlohmann::json{};
        },
        // UrlBuilder
        []() { return "http://mock"; },
        // HeaderBuilder
        []() { return std::map<std::string, std::string>{}; },
        // ResponseParser
        [](APIResponse&, const std::string&) {},
        options,
        false // no retry
    ); // mock will fail network, but lambda runs first
    
    assert(verified && "PayloadBuilder lambda was not called or assertions failed inside");
    std::cout << "Param Propagation passed.\n";
}

int main() {
    test_provider_bootstrap_env();
    test_chat_message_builder_images();
    test_response_handler_429();
    test_analysis_input_helpers(); // New
    test_param_propagation();      // New
    std::cout << "ALL ITERATION 2 TESTS PASSED\n";
    return 0;
}
