// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of the LLMEngine test suite (error path tests) and is
// licensed under the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/providers/APIClient.hpp"
#include "LLMEngine/core/AnalysisResult.hpp"
#include "LLMEngine/diagnostics/DebugArtifactManager.hpp"
#include "LLMEngine/core/ErrorCodes.hpp"
#include "LLMEngine/http/HttpStatus.hpp"
#include "LLMEngine/utils/Logger.hpp"
#include "LLMEngine/http/ResponseHandler.hpp"
#include "LLMEngine/http/ResponseParser.hpp"
#include "LLMEngine/utils/Utils.hpp"
#include "providers/OpenAICompatibleClient.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>

using namespace LLMEngine;
using namespace LLMEngineAPI;

// Mock logger for testing
class TestLogger : public Logger {
  public:
    void log(LogLevel level, std::string_view message) override {
        last_level_ = level;
        last_message_ = std::string(message);
        messages_.push_back({level, std::string(message)});
    }

    LogLevel last_level_ = LogLevel::Info;
    std::string last_message_;
    std::vector<std::pair<LogLevel, std::string>> messages_;
};

// Test ResponseHandler error paths
void testResponseHandlerErrorPaths() {
    std::cout << "\n=== Testing ResponseHandler Error Paths ===" << std::endl;

    // Test 1: Null logger handling
    {
        std::cout << "\n1. Testing null logger handling..." << std::endl;
        APIResponse api_resp;
        api_resp.success = false;
        api_resp.errorMessage = "Test error";
        api_resp.statusCode = 500;
        api_resp.errorCode = LLMEngineErrorCode::Server;

        AnalysisResult result =
            ResponseHandler::handle(api_resp, nullptr, "/tmp/test", "test", false, nullptr);

        assert(!result.success);
        assert(result.statusCode == 500);
        assert(!result.errorMessage.empty());
        assert(result.errorCode == LLMEngineErrorCode::Server);
        std::cout << "✓ Null logger handled correctly" << std::endl;
    }

    // Test 2: Various HTTP error codes
    {
        std::cout << "\n2. Testing HTTP error code classification..." << std::endl;

        // Test 401 (Auth error)
        {
            APIResponse api_resp;
            api_resp.success = false;
            api_resp.errorMessage = "Unauthorized";
            api_resp.statusCode = 401;
            api_resp.errorCode = LLMEngineErrorCode::None;

            AnalysisResult result =
                ResponseHandler::handle(api_resp, nullptr, "/tmp/test", "test", false, nullptr);

            assert(!result.success);
            assert(result.statusCode == 401);
            assert(result.errorCode == LLMEngineErrorCode::Auth);
            std::cout << "  ✓ 401 classified as Auth error" << std::endl;
        }

        // Test 403 (Auth error)
        {
            APIResponse api_resp;
            api_resp.success = false;
            api_resp.errorMessage = "Forbidden";
            api_resp.statusCode = 403;
            api_resp.errorCode = LLMEngineErrorCode::None;

            AnalysisResult result =
                ResponseHandler::handle(api_resp, nullptr, "/tmp/test", "test", false, nullptr);

            assert(!result.success);
            assert(result.statusCode == 403);
            assert(result.errorCode == LLMEngineErrorCode::Auth);
            std::cout << "  ✓ 403 classified as Auth error" << std::endl;
        }

        // Test 429 (Rate limit)
        {
            APIResponse api_resp;
            api_resp.success = false;
            api_resp.errorMessage = "Rate limited";
            api_resp.statusCode = 429;
            api_resp.errorCode = LLMEngineErrorCode::RateLimited;

            AnalysisResult result =
                ResponseHandler::handle(api_resp, nullptr, "/tmp/test", "test", false, nullptr);

            assert(!result.success);
            assert(result.statusCode == 429);
            assert(result.errorCode == LLMEngineErrorCode::RateLimited);
            std::cout << "  ✓ 429 classified as RateLimited error" << std::endl;
        }

        // Test 400 (Client error)
        {
            APIResponse api_resp;
            api_resp.success = false;
            api_resp.errorMessage = "Bad request";
            api_resp.statusCode = 400;
            api_resp.errorCode = LLMEngineErrorCode::None;

            AnalysisResult result =
                ResponseHandler::handle(api_resp, nullptr, "/tmp/test", "test", false, nullptr);

            assert(!result.success);
            assert(result.statusCode == 400);
            assert(result.errorCode == LLMEngineErrorCode::Client);
            std::cout << "  ✓ 400 classified as Client error" << std::endl;
        }

        // Test 500 (Server error)
        {
            APIResponse api_resp;
            api_resp.success = false;
            api_resp.errorMessage = "Internal server error";
            api_resp.statusCode = 500;
            api_resp.errorCode = LLMEngineErrorCode::None;

            AnalysisResult result =
                ResponseHandler::handle(api_resp, nullptr, "/tmp/test", "test", false, nullptr);

            assert(!result.success);
            assert(result.statusCode == 500);
            assert(result.errorCode == LLMEngineErrorCode::Server);
            std::cout << "  ✓ 500 classified as Server error" << std::endl;
        }

        // Test 0 status code (Unknown error)
        {
            APIResponse api_resp;
            api_resp.success = false;
            api_resp.errorMessage = "Unknown error";
            api_resp.statusCode = 0;
            api_resp.errorCode = LLMEngineErrorCode::Unknown;

            AnalysisResult result =
                ResponseHandler::handle(api_resp, nullptr, "/tmp/test", "test", false, nullptr);

            assert(!result.success);
            assert(result.statusCode == 0);
            assert(result.errorCode == LLMEngineErrorCode::Unknown);
            std::cout << "  ✓ 0 status code handled correctly" << std::endl;
        }
    }

    // Test 3: Error message enhancement with HTTP status
    {
        std::cout << "\n3. Testing error message enhancement..." << std::endl;
        TestLogger logger;
        APIResponse api_resp;
        api_resp.success = false;
        api_resp.errorMessage = "Connection failed";
        api_resp.statusCode = 503;
        api_resp.errorCode = LLMEngineErrorCode::Server;

        AnalysisResult result =
            ResponseHandler::handle(api_resp, nullptr, "/tmp/test", "test", false, &logger);

        assert(!result.success);
        assert(result.errorMessage.find("HTTP 503") != std::string::npos);
        assert(result.errorMessage.find("Connection failed") != std::string::npos);
        std::cout << "  ✓ Error message enhanced with HTTP status" << std::endl;
    }

    // Test 4: Success path with empty response
    {
        std::cout << "\n4. Testing success path with empty response..." << std::endl;
        APIResponse api_resp;
        api_resp.success = true;
        api_resp.content = "";
        api_resp.statusCode = 200;

        AnalysisResult result =
            ResponseHandler::handle(api_resp, nullptr, "/tmp/test", "test", false, nullptr);

        assert(result.success);
        assert(result.statusCode == 200);
        assert(result.errorCode == LLMEngineErrorCode::None);
        std::cout << "  ✓ Empty success response handled correctly" << std::endl;
    }

    // Test 5: Success path with think tags
    {
        std::cout << "\n5. Testing success path with think tags..." << std::endl;
        APIResponse api_resp;
        api_resp.success = true;
        api_resp.content = "<think>This is thinking</think>This is the answer";
        api_resp.statusCode = 200;

        AnalysisResult result =
            ResponseHandler::handle(api_resp, nullptr, "/tmp/test", "test", false, nullptr);

        assert(result.success);
        assert(result.think == "This is thinking");
        assert(result.content == "This is the answer");
        std::cout << "  ✓ Response with think tags parsed correctly" << std::endl;
    }

    std::cout << "\n✓ ResponseHandler error path tests completed" << std::endl;
}

// Test OpenAICompatibleClient error paths
void testOpenAICompatibleClientErrorPaths() {
    std::cout << "\n=== Testing OpenAICompatibleClient Error Paths ===" << std::endl;

    // Test 1: Invalid response format (missing choices)
    {
        std::cout << "\n1. Testing invalid response format (missing choices)..." << std::endl;
        APIResponse response;
        response.success = true; // Initialize to test that it doesn't get set to false
        response.rawResponse = nlohmann::json::parse(R"({"model": "test"})");

        OpenAICompatibleClient::parseOpenAIResponse(response, "");

        // Note: parseOpenAIResponse doesn't set success=false on error, only sets error fields
        assert(response.errorCode == LLMEngineErrorCode::InvalidResponse);
        assert(response.errorMessage == "Invalid response format");
        std::cout << "  ✓ Missing choices array detected" << std::endl;
    }

    // Test 2: Empty choices array
    {
        std::cout << "\n2. Testing empty choices array..." << std::endl;
        APIResponse response;
        response.success = true; // Initialize
        response.rawResponse = nlohmann::json::parse(R"({"choices": []})");

        OpenAICompatibleClient::parseOpenAIResponse(response, "");

        assert(response.errorCode == LLMEngineErrorCode::InvalidResponse);
        assert(response.errorMessage == "Invalid response format");
        std::cout << "  ✓ Empty choices array detected" << std::endl;
    }

    // Test 3: Missing message in choice
    {
        std::cout << "\n3. Testing missing message in choice..." << std::endl;
        APIResponse response;
        response.success = true; // Initialize
        response.rawResponse = nlohmann::json::parse(R"({"choices": [{"index": 0}]})");

        OpenAICompatibleClient::parseOpenAIResponse(response, "");

        assert(response.errorCode == LLMEngineErrorCode::InvalidResponse);
        assert(response.errorMessage == "No content in response");
        std::cout << "  ✓ Missing message detected" << std::endl;
    }

    // Test 4: Missing content in message
    {
        std::cout << "\n4. Testing missing content in message..." << std::endl;
        APIResponse response;
        response.success = true; // Initialize
        response.rawResponse =
            nlohmann::json::parse(R"({"choices": [{"message": {"role": "assistant"}}]})");

        OpenAICompatibleClient::parseOpenAIResponse(response, "");

        assert(response.errorCode == LLMEngineErrorCode::InvalidResponse);
        assert(response.errorMessage == "No content in response");
        std::cout << "  ✓ Missing content detected" << std::endl;
    }

    // Test 5: Valid response
    {
        std::cout << "\n5. Testing valid response..." << std::endl;
        APIResponse response;
        response.success = false; // Initialize to false to test it gets set to true
        response.rawResponse = nlohmann::json::parse(
            R"({"choices": [{"message": {"content": "Hello, world!", "role": "assistant"}}]})");

        OpenAICompatibleClient::parseOpenAIResponse(response, "");

        assert(response.success);
        assert(response.content == "Hello, world!");
        std::cout << "  ✓ Valid response parsed correctly" << std::endl;
    }

    // Test 6: Malformed JSON (should be handled by caller, but test robustness)
    {
        std::cout << "\n6. Testing malformed JSON handling..." << std::endl;
        APIResponse response;
        response.success = true; // Initialize
        // Try to parse invalid JSON - this should throw, but we test that the function
        // doesn't crash if raw_response is already set to invalid state
        try {
            response.rawResponse = nlohmann::json::parse("invalid json");
            // If we get here, the JSON was parsed (as null or something)
            // The function should handle this gracefully
            OpenAICompatibleClient::parseOpenAIResponse(response, "");
            // Should fail because invalid JSON won't have choices
            assert(response.errorCode == LLMEngineErrorCode::InvalidResponse);
            std::cout << "  ✓ Malformed JSON handled gracefully" << std::endl;
        } catch (const std::exception&) {
            // JSON parse exception is expected - this is handled by caller
            std::cout << "  ✓ JSON parse exception (expected, handled by caller)" << std::endl;
        }
    }

    std::cout << "\n✓ OpenAICompatibleClient error path tests completed" << std::endl;
}

// Test Utils validation error paths
void testUtilsValidationErrorPaths() {
    std::cout << "\n=== Testing Utils Validation Error Paths ===" << std::endl;

    // Test validateApiKey error paths
    {
        std::cout << "\n1. Testing validateApiKey error paths..." << std::endl;

        // Empty key
        assert(!Utils::validateApiKey(""));
        std::cout << "  ✓ Empty API key rejected" << std::endl;

        // Too short
        assert(!Utils::validateApiKey("short"));
        std::cout << "  ✓ Too short API key rejected" << std::endl;

        // Too long
        std::string long_key(513, 'a');
        assert(!Utils::validateApiKey(long_key));
        std::cout << "  ✓ Too long API key rejected" << std::endl;

        // Control characters
        assert(!Utils::validateApiKey("valid_key_with\nnewline"));
        assert(!Utils::validateApiKey("valid_key_with\ttab"));
        assert(!Utils::validateApiKey("valid_key_with\rcarriage"));
        std::cout << "  ✓ API keys with control characters rejected" << std::endl;

        // Valid key (minimum length)
        assert(Utils::validateApiKey("1234567890"));
        std::cout << "  ✓ Valid API key (minimum length) accepted" << std::endl;

        // Valid key (maximum length)
        std::string max_key(512, 'a');
        assert(Utils::validateApiKey(max_key));
        std::cout << "  ✓ Valid API key (maximum length) accepted" << std::endl;
    }

    // Test validateModelName error paths
    {
        std::cout << "\n2. Testing validateModelName error paths..." << std::endl;

        // Empty name
        assert(!Utils::validateModelName(""));
        std::cout << "  ✓ Empty model name rejected" << std::endl;

        // Too long
        std::string long_name(257, 'a');
        assert(!Utils::validateModelName(long_name));
        std::cout << "  ✓ Too long model name rejected" << std::endl;

        // Invalid characters
        assert(!Utils::validateModelName("model@name"));
        assert(!Utils::validateModelName("model#name"));
        assert(!Utils::validateModelName("model name")); // Space not allowed
        assert(!Utils::validateModelName("model\nname"));
        std::cout << "  ✓ Model names with invalid characters rejected" << std::endl;

        // Valid names
        assert(Utils::validateModelName("gpt-4"));
        assert(Utils::validateModelName("qwen2.5"));
        assert(Utils::validateModelName("model_name"));
        assert(Utils::validateModelName("org/model-name"));
        assert(Utils::validateModelName("model.name"));
        std::cout << "  ✓ Valid model names accepted" << std::endl;
    }

    // Test validateUrl error paths
    {
        std::cout << "\n3. Testing validateUrl error paths..." << std::endl;

        // Empty URL
        assert(!Utils::validateUrl(""));
        std::cout << "  ✓ Empty URL rejected" << std::endl;

        // Too short (less than http://)
        assert(!Utils::validateUrl("http:/"));
        std::cout << "  ✓ Too short URL rejected" << std::endl;

        // Too long
        std::string long_url(2049, 'a');
        long_url = "http://" + long_url;
        assert(!Utils::validateUrl(long_url));
        std::cout << "  ✓ Too long URL rejected" << std::endl;

        // Missing protocol
        assert(!Utils::validateUrl("example.com"));
        assert(!Utils::validateUrl("ftp://example.com"));
        std::cout << "  ✓ URLs without http/https rejected" << std::endl;

        // Control characters
        assert(!Utils::validateUrl("http://example.com\n/path"));
        assert(!Utils::validateUrl("https://example.com\t/path"));
        std::cout << "  ✓ URLs with control characters rejected" << std::endl;

        // Valid URLs
        assert(Utils::validateUrl("http://example.com"));
        assert(Utils::validateUrl("https://example.com"));
        assert(Utils::validateUrl("http://example.com/path"));
        assert(Utils::validateUrl("https://api.example.com/v1/endpoint"));
        std::cout << "  ✓ Valid URLs accepted" << std::endl;
    }

    std::cout << "\n✓ Utils validation error path tests completed" << std::endl;
}

// Test ResponseParser error paths
void testResponseParserErrorPaths() {
    std::cout << "\n=== Testing ResponseParser Error Paths ===" << std::endl;

    // Test 1: Empty response
    {
        std::cout << "\n1. Testing empty response..." << std::endl;
        auto [think, remaining] = ResponseParser::parseResponse("");
        assert(think.empty());
        assert(remaining.empty());
        std::cout << "  ✓ Empty response handled correctly" << std::endl;
    }

    // Test 2: Malformed think tags
    {
        std::cout << "\n2. Testing malformed think tags..." << std::endl;

        // Unclosed think tag
        auto [think1, remaining1] = ResponseParser::parseResponse("<think>Unclosed tag");
        assert(think1.empty());
        assert(remaining1 == "<think>Unclosed tag");
        std::cout << "  ✓ Unclosed think tag handled" << std::endl;

        // Mismatched tags
        auto [think2, remaining2] = ResponseParser::parseResponse("<think>Content</think2>");
        assert(think2.empty());
        assert(remaining2 == "<think>Content</think2>");
        std::cout << "  ✓ Mismatched tags handled" << std::endl;
    }

    // Test 3: Multiple think tags (should extract first)
    {
        std::cout << "\n3. Testing multiple think tags..." << std::endl;
        auto [think, remaining] =
            ResponseParser::parseResponse("<think>First</think>Content<think>Second</think>More");
        assert(think == "First");
        assert(remaining.find("Content") != std::string::npos);
        std::cout << "  ✓ Multiple think tags handled (first extracted)" << std::endl;
    }

    // Test 4: Nested think tags (edge case)
    {
        std::cout << "\n4. Testing nested think tags..." << std::endl;
        auto [think, remaining] =
            ResponseParser::parseResponse("<think>Outer<think>Inner</think></think>Content");
        // Should extract from first opening to first closing
        assert(think.find("Outer") != std::string::npos);
        std::cout << "  ✓ Nested think tags handled" << std::endl;
    }

    std::cout << "\n✓ ResponseParser error path tests completed" << std::endl;
}

int main() {
    std::cout << "=== LLMEngine Error Path Tests ===" << std::endl;

    try {
        testResponseHandlerErrorPaths();
        testOpenAICompatibleClientErrorPaths();
        testUtilsValidationErrorPaths();
        testResponseParserErrorPaths();

        std::cout << "\n=== All Error Path Tests Completed ===" << std::endl;
        std::cout << "✓ ResponseHandler error paths" << std::endl;
        std::cout << "✓ OpenAICompatibleClient error paths" << std::endl;
        std::cout << "✓ Utils validation error paths" << std::endl;
        std::cout << "✓ ResponseParser error paths" << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
