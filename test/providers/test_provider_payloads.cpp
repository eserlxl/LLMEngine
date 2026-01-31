#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "LLMEngine/providers/APIClient.hpp"
#include <nlohmann/json.hpp>

using namespace LLMEngineAPI;
using Json = nlohmann::json;

namespace {
constexpr double kExpectedTemp = 0.5;
constexpr double kExpectedTempHigh = 0.7;
constexpr int kExpectedMaxTokens = 100;
constexpr int kExpectedMaxTokensLarge = 1024;
constexpr double kExpectedTopP = 1.0;
}

// Testable subclasses to expose protected methods
class TestableGeminiClient : public GeminiClient {
public:
    using GeminiClient::GeminiClient;
    using GeminiClient::buildPayload;
    using GeminiClient::buildUrl;
    using GeminiClient::buildHeaders;
};

class TestableAnthropicClient : public AnthropicClient {
public:
    using AnthropicClient::AnthropicClient;
    using AnthropicClient::buildPayload;
    using AnthropicClient::buildUrl;
    using AnthropicClient::buildHeaders;
};

class TestableOllamaClient : public OllamaClient {
public:
    using OllamaClient::OllamaClient;
    using OllamaClient::buildPayload;
    using OllamaClient::buildUrl;
    using OllamaClient::buildHeaders;
};

class TestableOpenAIClient : public OpenAIClient {
public:
    using OpenAIClient::OpenAIClient;
    using OpenAIClient::buildPayload;
    using OpenAIClient::buildUrl;
    using OpenAIClient::buildHeaders;
};

class TestableQwenClient : public QwenClient {
public:
    using QwenClient::QwenClient;
    using QwenClient::buildPayload;
    using QwenClient::buildUrl;
    using QwenClient::buildHeaders;
};

TEST(GeminiClientTest, PayloadConstruction) {
    TestableGeminiClient client("test_key", "gemini-pro");
    Json input;
    Json params = {{"temperature", kExpectedTemp}, {"max_tokens", kExpectedMaxTokens}, {"top_p", kExpectedTopP}};
    
    // Test Build URL
    std::string url = client.buildUrl(true);
    // GeminiClient uses header-based auth and v1 base URL by default
    EXPECT_EQ(url, "https://generativelanguage.googleapis.com/v1/models/gemini-pro:streamGenerateContent?alt=sse");
    
    // Test Build Headers
    auto headers = client.buildHeaders();
    EXPECT_EQ(headers["Content-Type"], "application/json");

    // Test Payload
    Json payload = client.buildPayload("Hello", input, params);
    EXPECT_TRUE(payload.contains("contents"));
    EXPECT_EQ(payload["contents"][0]["parts"][0]["text"], "Hello");
    EXPECT_EQ(payload["generationConfig"]["temperature"], kExpectedTemp);
}

TEST(AnthropicClientTest, PayloadConstruction) {
    TestableAnthropicClient client("test_key", "claude-3-opus-20240229");
    Json input = {{"system_prompt", "You are a helper."}};
    Json params = {{"max_tokens", kExpectedMaxTokensLarge}, {"temperature", kExpectedTempHigh}, {"top_p", kExpectedTopP}};

    // Test Build URL
    EXPECT_EQ(client.buildUrl(), "https://api.anthropic.com/v1/messages");

    // Test Build Headers
    auto headers = client.buildHeaders();
    EXPECT_EQ(headers["x-api-key"], "test_key");
    EXPECT_EQ(headers["anthropic-version"], "2023-06-01");

    // Test Build Payload
    Json payload = client.buildPayload("Hello", input, params);
    EXPECT_EQ(payload["model"], "claude-3-opus-20240229");
    EXPECT_EQ(payload["messages"][0]["role"], "user");
    EXPECT_EQ(payload["messages"][0]["content"], "Hello");
    EXPECT_EQ(payload["max_tokens"], kExpectedMaxTokensLarge);
    EXPECT_EQ(payload["system"], "You are a helper.");
}

TEST(OllamaClientTest, PayloadConstruction) {
    TestableOllamaClient client("http://localhost:11434", "llama3");
    Json input;
    Json params = {{"temperature", kExpectedTempHigh}};
    
    // Test Payload
    Json payload = client.buildPayload("Hello", input, params, false);
    EXPECT_EQ(payload["model"], "llama3");
    // Ollama chat format
    if (payload.contains("messages")) {
         EXPECT_EQ(payload["messages"][0]["content"], "Hello");
    } else {
         // Generate format?
         EXPECT_EQ(payload["prompt"], "Hello");
    }
}

TEST(OpenAIClientTest, PayloadConstruction) {
    TestableOpenAIClient client("test_key", "gpt-4");
    Json input;
    Json params = {{"temperature", kExpectedTempHigh}, 
                   {"max_tokens", kExpectedMaxTokensLarge}, 
                   {"top_p", kExpectedTopP},
                   {"frequency_penalty", 0.0},
                   {"presence_penalty", 0.0}};

    // Test Build URL
    EXPECT_EQ(client.buildUrl(), "https://api.openai.com/v1/chat/completions");
    
    // Test Build Headers
    auto headers = client.buildHeaders();
    EXPECT_EQ(headers["Authorization"], "Bearer test_key");

    // Test Build Payload
    Json payload = client.buildPayload("Hello", input, params);
    EXPECT_EQ(payload["model"], "gpt-4");
    EXPECT_EQ(payload["messages"][0]["role"], "user");
    EXPECT_EQ(payload["messages"][0]["content"], "Hello");
    EXPECT_EQ(payload["temperature"], kExpectedTempHigh);
    EXPECT_EQ(payload["max_tokens"], kExpectedMaxTokensLarge);
    EXPECT_EQ(payload["top_p"], kExpectedTopP);
}

TEST(QwenClientTest, PayloadConstruction) {
    TestableQwenClient client("qwen-key", "qwen-turbo");
    Json input;
    Json params = {{"temperature", kExpectedTempHigh}, 
                   {"max_tokens", kExpectedMaxTokensLarge}, 
                   {"top_p", kExpectedTopP},
                   {"frequency_penalty", 0.0},
                   {"presence_penalty", 0.0}};

    // Qwen uses OpenAI compatible base URL structure but different base?
    // Default Qwen base is https://dashscope-intl.aliyuncs.com/compatible-mode/v1
    EXPECT_EQ(client.buildUrl(), "https://dashscope-intl.aliyuncs.com/compatible-mode/v1/chat/completions");

    auto headers = client.buildHeaders();
    EXPECT_EQ(headers["Authorization"], "Bearer qwen-key");

    Json payload = client.buildPayload("Hello", input, params);
    EXPECT_EQ(payload["model"], "qwen-turbo");
}
