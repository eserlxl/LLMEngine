// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of the LLMEngine test suite (ChatCompletionRequestHelper tests) and is
// licensed under the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "../src/ChatCompletionRequestHelper.hpp"
#include "LLMEngine/Constants.hpp"

#include <cassert>
#include <iostream>
#include <nlohmann/json.hpp>

using namespace LLMEngineAPI;

void testChatMessageBuilderBasic() {
    nlohmann::json input = {};
    std::string prompt = "Hello, world!";

    auto messages = ChatMessageBuilder::buildMessages(prompt, input);

    assert(messages.is_array());
    assert(messages.size() == 1);
    assert(messages[0]["role"] == "user");
    assert(messages[0]["content"] == "Hello, world!");

    std::cout << "✓ Basic message building test passed\n";
}

void testChatMessageBuilderWithSystemPrompt() {
    nlohmann::json input = {{std::string(LLMEngine::Constants::JsonKeys::SYSTEM_PROMPT),
                             "You are a helpful assistant."}};
    std::string prompt = "What is 2+2?";

    auto messages = ChatMessageBuilder::buildMessages(prompt, input);

    assert(messages.is_array());
    assert(messages.size() == 2);
    assert(messages[0]["role"] == "system");
    assert(messages[0]["content"] == "You are a helpful assistant.");
    assert(messages[1]["role"] == "user");
    assert(messages[1]["content"] == "What is 2+2?");

    std::cout << "✓ System prompt message building test passed\n";
}

void testChatMessageBuilderWithNumericSystemPrompt() {
    nlohmann::json input = {{std::string(LLMEngine::Constants::JsonKeys::SYSTEM_PROMPT), 42}};
    std::string prompt = "Test";

    auto messages = ChatMessageBuilder::buildMessages(prompt, input);

    assert(messages.is_array());
    assert(messages.size() == 2);
    assert(messages[0]["role"] == "system");
    assert(messages[0]["content"] == "42"); // Converted to string
    assert(messages[1]["role"] == "user");

    std::cout << "✓ Numeric system prompt test passed\n";
}

void testChatMessageBuilderWithBooleanSystemPrompt() {
    nlohmann::json input = {{std::string(LLMEngine::Constants::JsonKeys::SYSTEM_PROMPT), true}};
    std::string prompt = "Test";

    auto messages = ChatMessageBuilder::buildMessages(prompt, input);

    assert(messages.is_array());
    assert(messages.size() == 2);
    assert(messages[0]["role"] == "system");
    assert(messages[0]["content"] == "true"); // Converted to string
    assert(messages[1]["role"] == "user");

    std::cout << "✓ Boolean system prompt test passed\n";
}

void testChatMessageBuilderWithObjectSystemPrompt() {
    // Object system prompts should be ignored (non-string, non-scalar)
    nlohmann::json input = {
        {std::string(LLMEngine::Constants::JsonKeys::SYSTEM_PROMPT), {{"key", "value"}}}};
    std::string prompt = "Test";

    auto messages = ChatMessageBuilder::buildMessages(prompt, input);

    assert(messages.is_array());
    assert(messages.size() == 1); // Only user message, system prompt ignored
    assert(messages[0]["role"] == "user");

    std::cout << "✓ Object system prompt (ignored) test passed\n";
}

void testChatMessageBuilderWithArraySystemPrompt() {
    // Array system prompts should be ignored (non-string, non-scalar)
    nlohmann::json input = {
        {std::string(LLMEngine::Constants::JsonKeys::SYSTEM_PROMPT), nlohmann::json::array()}};
    std::string prompt = "Test";

    auto messages = ChatMessageBuilder::buildMessages(prompt, input);

    assert(messages.is_array());
    assert(messages.size() == 1); // Only user message, system prompt ignored
    assert(messages[0]["role"] == "user");

    std::cout << "✓ Array system prompt (ignored) test passed\n";
}

void testChatMessageBuilderEmptyPrompt() {
    nlohmann::json input = {};
    std::string prompt = "";

    auto messages = ChatMessageBuilder::buildMessages(prompt, input);

    assert(messages.is_array());
    assert(messages.is_array());
    assert(messages.size() == 0); // No user message if prompt is empty

    std::cout << "✓ Empty prompt test passed\n";
}

int main() {
    std::cout << "=== ChatCompletionRequestHelper Tests ===\n";

    testChatMessageBuilderBasic();
    testChatMessageBuilderWithSystemPrompt();
    testChatMessageBuilderWithNumericSystemPrompt();
    testChatMessageBuilderWithBooleanSystemPrompt();
    testChatMessageBuilderWithObjectSystemPrompt();
    testChatMessageBuilderWithArraySystemPrompt();
    testChatMessageBuilderEmptyPrompt();

    std::cout << "\nAll ChatCompletionRequestHelper tests passed!\n";
    return 0;
}
