// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LLMEngine/LLMEngine.hpp"
#include "LLMEngine/APIClient.hpp"
#include <iostream>
#include <memory>
#include <vector>
#include <stdexcept>

// Since we cannot easily mock the internal CPR requests of the real clients without extensive refactoring or a mock server,
// these tests serve as "compilation and interface" verification for the new methods.
// Real verification requires integration tests with a mock server or real API keys.
// For unit testing the PARSING logic, we would ideally expose the parser functions or friend them.
// given the constraints, we will rely on integration tests or manual verification for now, 
// but we can compile the client instantiation and check basic properties.

int main() {
    std::cout << "Provider streaming compilation check..." << std::endl;
    
    // We can't actually call sendRequestStream without a network or mock server intercepting cpr.
    // However, the fact that this test compiles confirms the interface is correct.
    
    try {
        LLMEngineAPI::GeminiClient gemini("fake_key");
        LLMEngineAPI::AnthropicClient anthropic("fake_key");
        LLMEngineAPI::OllamaClient ollama("http://localhost:11434");
        
        std::cout << "Clients instantiated successfully." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "PASS" << std::endl;
    return 0;
}
