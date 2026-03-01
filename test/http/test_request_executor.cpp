#include <gtest/gtest.h>
#include "llmengine/providers/i_request_executor.hpp"
#include "../support/fake_api_client.hpp"

using namespace LLMEngine;
using namespace LLMEngineAPI;

class RequestExecutorTest : public ::testing::Test {
  protected:
    DefaultRequestExecutor executor;
    FakeAPIClient fake_client;
};

TEST_F(RequestExecutorTest, ExecuteValidClient) {
    APIResponse mock_resp;
    mock_resp.success = true;
    mock_resp.content = "Success content";
    fake_client.setNextResponse(mock_resp);

    RequestOptions options;
    options.timeout_ms = 1234;

    APIResponse resp = executor.execute(&fake_client, "test prompt", {{"key", "val"}}, {{"param1", 1}}, options);

    EXPECT_TRUE(resp.success);
    EXPECT_EQ(resp.content, "Success content");
    EXPECT_EQ(fake_client.getLastOptions().timeout_ms, 1234);
}

TEST_F(RequestExecutorTest, ExecuteNullClient) {
    RequestOptions options;
    APIResponse resp = executor.execute(nullptr, "test prompt", {}, {}, options);

    EXPECT_FALSE(resp.success);
    EXPECT_EQ(resp.statusCode, 500);
    EXPECT_EQ(resp.errorMessage, "API client not initialized");
    EXPECT_EQ(resp.errorCode, LLMEngineErrorCode::Unknown);
}

TEST_F(RequestExecutorTest, ExecuteStreamValidClient) {
    fake_client.setNextStreamChunks({"chunk1", "chunk2"});
    
    std::vector<std::string> received;
    StreamCallback cb = [&received](const StreamChunk& chunk) {
        if (!chunk.content.empty()) {
            received.push_back(std::string(chunk.content));
        }
    };

    RequestOptions options;
    executor.executeStream(&fake_client, "prompt", {}, {}, std::move(cb), options);

    ASSERT_EQ(received.size(), 2);
    EXPECT_EQ(received[0], "chunk1");
    EXPECT_EQ(received[1], "chunk2");
}

TEST_F(RequestExecutorTest, ExecuteStreamNullClient) {
    RequestOptions options;
    StreamCallback cb = [](const StreamChunk&) {};

    EXPECT_THROW(executor.executeStream(nullptr, "prompt", {}, {}, std::move(cb), options), std::runtime_error);
}
