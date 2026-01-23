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

int main() {
    test_provider_bootstrap_env();
    test_chat_message_builder_images();
    test_response_handler_429();
    std::cout << "ALL ITERATION 2 TESTS PASSED\n";
    return 0;
}
