#include "LLMEngine/LLMEngine.hpp"
#include "LLMEngine/ToolBuilder.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace LLMEngine;
using namespace testing;

// Use full qualification to avoid ambiguity
class MockInterceptor : public LLMEngine::LLMEngine::IInterceptor {
public:
    MOCK_METHOD(void, onRequest, (LLMEngine::RequestContext&), (override));
    MOCK_METHOD(void, onResponse, (LLMEngine::AnalysisResult&), (override));
};

// Fake client to avoid network calls
class FakeClient : public LLMEngineAPI::APIClient {
public:
    FakeClient() {} // APIClient has default ctor
    
    // Match signature from APIClient.hpp
    LLMEngineAPI::APIResponse sendRequest(std::string_view, const nlohmann::json&,
                                          const nlohmann::json&,
                                          const LLMEngine::RequestOptions&) const override {
        return {
            .success = true,
            .content = "response",
            .error_message = "",
            .status_code = 200,
            .raw_response = {},
            .error_code = LLMEngine::LLMEngineErrorCode::None,
            .usage = {},
            .finish_reason = "stop"
        };
    }

    void sendRequestStream(std::string_view, const nlohmann::json&,
                           const nlohmann::json&,
                           LLMEngine::StreamCallback callback,
                           const LLMEngine::RequestOptions&) const override {
        LLMEngine::StreamChunk chunk;
        chunk.content = "stream response";
        chunk.is_done = false;
        callback(chunk);
        
        chunk.content = "";
        chunk.is_done = true;
        callback(chunk);
    }
    
    // Missing pure virtuals
    std::string getProviderName() const override { return "fake"; }
    LLMEngineAPI::ProviderType getProviderType() const override { return LLMEngineAPI::ProviderType::OPENAI; }
};

class ValidationTest : public Test {
protected:
    std::unique_ptr<LLMEngine::LLMEngine> engine;
    std::shared_ptr<MockInterceptor> interceptor;

    void SetUp() override {
        auto client = std::make_unique<FakeClient>();
        engine = std::make_unique<LLMEngine::LLMEngine>(std::move(client));
        interceptor = std::make_shared<MockInterceptor>();
        engine->addInterceptor(interceptor);
    }
};

TEST_F(ValidationTest, AnalyzeThrowsOnInvalidToolChoice) {
    AnalysisInput input;
    input.withUserMessage("Use the tool");
    
    // Invalid tool choice: refers to "my_tool" but no tools defined
    input.withToolChoice(ToolChoice::function("my_tool"));

    EXPECT_THROW({
        (void)engine->analyze(input, "test");
    }, std::invalid_argument);
}

TEST_F(ValidationTest, AnalyzeStreamThrowsOnInvalidToolChoice) {
    AnalysisInput input;
    input.withUserMessage("Use the tool");
    input.withToolChoice(ToolChoice::function("my_tool")); // Invalid

    EXPECT_THROW({
        engine->analyzeStream("prompt", input.toJson(), "test", {}, [](auto&&){});
    }, std::invalid_argument);
}

TEST_F(ValidationTest, AnalyzeStreamCallsInterceptors) {
    AnalysisInput input;
    input.withUserMessage("Hello");

    // Expect onRequest to be called
    EXPECT_CALL(*interceptor, onRequest(_)).Times(1);
    
    // Based on implementation, onResponse is NOT called for stream yet
    EXPECT_CALL(*interceptor, onResponse(_)).Times(0);

    bool callbackCalled = false;
    engine->analyzeStream("Hello", input.toJson(), "test", {}, [&](const StreamChunk& chunk) {
        if (!chunk.content.empty()) callbackCalled = true;
    });

    EXPECT_TRUE(callbackCalled);
}

TEST_F(ValidationTest, AnalyzeJsonOverloadThrowsOnInvalidToolChoice) {
    AnalysisInput input;
    input.withUserMessage("Use the tool");
    input.withToolChoice(ToolChoice::function("my_tool")); // Invalid
    
    EXPECT_THROW({
        (void)engine->analyze("prompt", input.toJson(), "test");
    }, std::invalid_argument);
}

TEST_F(ValidationTest, ValidToolChoiceDoesNotThrow) {
    AnalysisInput input;
    input.withUserMessage("Use the tool");
    
    // Define the tool
    input.addTool(ToolBuilder::createFunction("my_tool", "description"));
    
    // Valid tool choice
    input.withToolChoice(ToolChoice::function("my_tool"));

    // Should not throw
    EXPECT_NO_THROW({
        (void)engine->analyze(input, "test");
    });
}
