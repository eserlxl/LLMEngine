#include <gtest/gtest.h>
#include "llmengine/http/response_handler.hpp"
#include "llmengine/providers/api_client.hpp"
#include "llmengine/core/analysis_result.hpp"

using namespace LLMEngine;

TEST(ResponseHandlerTest, HandleErrorResponseUnknownCode) {
    LLMEngineAPI::APIResponse api_resp;
    api_resp.success = false;
    api_resp.statusCode = 429;
    api_resp.errorMessage = "Rate limit exceeded";
    api_resp.errorCode = LLMEngineErrorCode::None;

    AnalysisResult res = ResponseHandler::handle(api_resp, nullptr, "", "", false, nullptr);

    EXPECT_FALSE(res.success);
    EXPECT_EQ(res.errorCode, LLMEngineErrorCode::RateLimited);
    EXPECT_NE(res.errorMessage.find("429"), std::string::npos);
}

TEST(ResponseHandlerTest, HandleErrorResponseServerCode) {
    LLMEngineAPI::APIResponse api_resp;
    api_resp.success = false;
    api_resp.statusCode = 502;
    api_resp.errorMessage = "Bad Gateway";
    api_resp.errorCode = LLMEngineErrorCode::None;

    AnalysisResult res = ResponseHandler::handle(api_resp, nullptr, "", "", false, nullptr);

    EXPECT_FALSE(res.success);
    EXPECT_EQ(res.errorCode, LLMEngineErrorCode::Server);
}

TEST(ResponseHandlerTest, HandleSuccessResponseWithLogic) {
    LLMEngineAPI::APIResponse api_resp;
    api_resp.success = true;
    api_resp.statusCode = 200;
    api_resp.content = "<think>planning</think>answer";

    AnalysisResult res = ResponseHandler::handle(api_resp, nullptr, "", "", false, nullptr);

    EXPECT_TRUE(res.success);
    EXPECT_EQ(res.think, "planning");
    EXPECT_EQ(res.content, "answer");
}

TEST(ResponseHandlerTest, HandleSuccessResponseToolCallsExtract) {
    LLMEngineAPI::APIResponse api_resp;
    api_resp.success = true;
    api_resp.statusCode = 200;
    api_resp.content = "Function call happening";
    
    api_resp.rawResponse = R"({
        "choices": [{
            "message": {
                "tool_calls": [{
                    "id": "call_123",
                    "function": {
                        "name": "get_weather",
                        "arguments": "{\"loc\":\"Paris\"}"
                    }
                }]
            }
        }]
    })"_json;

    AnalysisResult res = ResponseHandler::handle(api_resp, nullptr, "", "", false, nullptr);

    EXPECT_TRUE(res.success);
    ASSERT_EQ(res.toolCalls.size(), 1);
    EXPECT_EQ(res.toolCalls[0].id, "call_123");
    EXPECT_EQ(res.toolCalls[0].name, "get_weather");
    EXPECT_EQ(res.toolCalls[0].arguments, "{\"loc\":\"Paris\"}");
}
