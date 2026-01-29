// Copyright © 2026 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LLMEngine/core/AnalysisInput.hpp"
#include "LLMEngine/core/LLMEngine.hpp"
#include "LLMEngine/core/ToolBuilder.hpp"
#include "support/FakeAPIClient.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>

using namespace LLMEngine;

void test_tool_choice_helpers() {
    std::cout << "Testing ToolChoice helpers..." << std::endl;
    
    auto none = ToolChoice::none();
    assert(none == "none");
    
    auto autoChoice = ToolChoice::autoChoice();
    assert(autoChoice == "auto");
    
    auto required = ToolChoice::required();
    assert(required == "required");
    
    auto func = ToolChoice::function("my_tool");
    assert(func["type"] == "function");
    assert(func["function"]["name"] == "my_tool");
    
    std::cout << "PASS" << std::endl;
}

void test_cancellation_token_factory() {
    std::cout << "Testing CancellationToken factory..." << std::endl;
    
    auto token = LLMEngine::LLMEngine::createCancellationToken();
    assert(token != nullptr);
    assert(!token->isCancelled());
    
    token->cancel();
    assert(token->isCancelled());
    
    std::cout << "PASS" << std::endl;
}

int main() {
    try {
        test_tool_choice_helpers();
        test_cancellation_token_factory();
        std::cout << "All Iteration 2 feature tests passed." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
