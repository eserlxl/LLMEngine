// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include <iostream>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "LLMEngine.hpp"
#include "LLMOutputProcessor.hpp"
#include "Utils.hpp"

int main() {
    std::cout << "=== LLMEngine Library Test ===" << std::endl;
    
    try {
        // Test 1: Basic LLMEngine initialization with payload
        std::cout << "\n1. Testing LLMEngine initialization with payload..." << std::endl;
        
        // Initialize LLMEngine with Ollama provider
        // Note: You'll need to have an Ollama server running for this to work
        std::string model = "ertghiu256/qwen3-4b-code-reasoning";  // Change this to your available model
        int log_retention_hours = 24;
        bool debug = true;  // Enable debug mode for testing
        
        // Create model parameters payload
        nlohmann::json model_params = {
            {"temperature", 0.7},
            {"think", false},
            {"context_window", 10000},
            {"top_p", 0.9},
            {"top_k", 40},
            {"min_p", 0.05}
        };
        
        LLMEngine engine(::LLMEngineAPI::ProviderType::OLLAMA, "", model, model_params, log_retention_hours, debug);
        std::cout << "✓ LLMEngine initialized successfully with payload" << std::endl;
        
        // Test 1.5: Test optional parameters (empty payload)
        std::cout << "\n1.5. Testing optional parameters with empty payload..." << std::endl;
        
        // Create engine with empty payload to test default behavior
        nlohmann::json empty_params = {};
        LLMEngine engine_empty(::LLMEngineAPI::ProviderType::OLLAMA, "", model, empty_params, log_retention_hours, debug);
        std::cout << "✓ LLMEngine with empty payload initialized successfully" << std::endl;
        std::cout << "Note: Empty payload means Ollama will use all default values" << std::endl;
        
        // Test 1.6: Test partial parameters payload
        std::cout << "\n1.6. Testing partial parameters payload..." << std::endl;
        
        // Create engine with only some parameters
        nlohmann::json partial_params = {
            {"temperature", 0.5},
            {"top_p", 0.8}
        };
        LLMEngine engine_partial(::LLMEngineAPI::ProviderType::OLLAMA, "", model, partial_params, log_retention_hours, debug);
        std::cout << "✓ LLMEngine with partial payload initialized successfully" << std::endl;
        std::cout << "Note: Only specified parameters will be sent to Ollama" << std::endl;
        
        // Test 2: Basic analysis call
        std::cout << "\n2. Testing basic analysis..." << std::endl;
        
        std::string prompt = "Write a simple C++ function that calculates the factorial of a number:";
        nlohmann::json input_data = {
            {"language", "C++"},
            {"function_name", "factorial"},
            {"input_type", "int"},
            {"return_type", "int"}
        };
        std::string analysis_type = "test_analysis";
        
        // Add max_tokens to input payload for testing
        input_data["max_tokens"] = 100;  // Limit tokens for testing
        
        AnalysisResult result = engine.analyze(prompt, input_data, analysis_type);
        
        if (result.success) {
            std::cout << "✓ Analysis completed successfully" << std::endl;
            std::cout << "Think section length: " << result.think.length() << " characters" << std::endl;
            std::cout << "Response section length: " << result.content.length() << " characters" << std::endl;
            
            // Display first 200 characters of each section
            std::cout << "\nThink section preview: " << std::endl;
            std::cout << result.think.substr(0, std::min(200UL, result.think.length())) << "..." << std::endl;
            
            std::cout << "\nResponse section preview: " << std::endl;
            std::cout << result.content.substr(0, std::min(200UL, result.content.length())) << "..." << std::endl;
        } else {
            std::cout << "✗ Analysis failed: " << result.errorMessage << " (status " << result.statusCode << ")" << std::endl;
        }
        
        // Test 3: LLMOutputProcessor usage
        std::cout << "\n3. Testing LLMOutputProcessor..." << std::endl;
        
        // Create a sample JSON response for testing the processor
        nlohmann::json sample_response = {
            {"analysis", "This is a test analysis"},
            {"risks", "Low risk detected"},
            {"recommendations", "Continue monitoring"}
        };
        
        LLMOutputProcessor processor(sample_response.dump(), true);
        std::cout << "✓ LLMOutputProcessor initialized successfully" << std::endl;
        
        // Test processor methods
        std::string raw_analysis = processor.getRawAnalysis();
        std::cout << "Raw analysis length: " << raw_analysis.length() << " characters" << std::endl;
        
        bool has_errors = processor.hasErrors();
        std::cout << "Has errors: " << (has_errors ? "Yes" : "No") << std::endl;
        
        std::vector<std::string> available_sections = processor.getAvailableSections();
        std::cout << "Available sections: " << available_sections.size() << std::endl;
        
        // Test 4: Utils functions
        std::cout << "\n4. Testing Utils functions..." << std::endl;
        
        // Test stripMarkdown function
        std::string markdown_text = "**Bold text** and *italic text*";
        std::string stripped = Utils::stripMarkdown(markdown_text);
        std::cout << "Original: " << markdown_text << std::endl;
        std::cout << "Stripped: " << stripped << std::endl;
        
        // Test TMP_DIR
        std::cout << "Temporary directory: " << Utils::TMP_DIR << std::endl;
        
        std::cout << "\n=== All tests completed successfully! ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error during testing: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
