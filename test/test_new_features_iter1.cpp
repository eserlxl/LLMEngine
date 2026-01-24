#include "support/FakeAPIClient.hpp"
#include "LLMEngine/APIClient.hpp"
#include "LLMEngine/AnalysisInput.hpp"
#include "LLMEngine/AnalysisResult.hpp"
#include "LLMEngine/LLMEngine.hpp"
#include "LLMEngine/Utils.hpp"
#include <cassert>
#include <future>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <thread>
#include <vector>

using namespace LLMEngine;
using namespace LLMEngineAPI;

void test_analysis_input_messages() {
    std::cout << "Running test_analysis_input_messages..." << std::endl;
    auto input = AnalysisInput::builder()
                     .withMessage("system", "You are a helper.")
                     .withMessage("user", "Hello");
    nlohmann::json json = input.toJson();
    assert(json.contains("messages"));
    assert(json["messages"].size() == 2);
    std::cout << "PASS" << std::endl;
}

void test_image_loading() {
    std::cout << "Running test_image_loading..." << std::endl;
    
    std::string filename = "test_image.png";
    std::ofstream ofs(filename, std::ios::binary);
    std::vector<uint8_t> data = {0x89, 0x50, 0x4E, 0x47};
    ofs.write(reinterpret_cast<const char*>(data.data()), data.size());
    ofs.close();

    AnalysisInput input;
    input.withImageFromFile(filename);

    nlohmann::json json = input.toJson();
    assert(json.contains("images"));
    assert(json["images"].is_array());
    assert(json["images"].size() == 1);
    
    std::string imgUrl = json["images"][0].get<std::string>();
    assert(imgUrl.find("data:image/png;base64,") == 0);
    
    std::filesystem::remove(filename);
    std::cout << "PASS" << std::endl;
}

void test_request_options_injection() {
    std::cout << "Running test_request_options_injection..." << std::endl;

    auto clientPtr = std::make_unique<FakeAPIClient>();
    FakeAPIClient* fakeClient = clientPtr.get();
    LLMEngine::LLMEngine engine(std::move(clientPtr), nlohmann::json{}, 24, false, nullptr);

    RequestOptions options;
    options.generation.user = "custom_user_123";
    options.generation.logprobs = true;
    options.generation.top_logprobs = 5;

    AnalysisInput input;
    input.withUserMessage("hi");

    auto result = engine.analyze("hi", input.toJson(), "test", options);
    (void)result;

    // Verify properties are passed via Options to the client
    const auto& lastOpts = fakeClient->getLastOptions();
    
    assert(lastOpts.generation.user.has_value());
    assert(*lastOpts.generation.user == "custom_user_123");
    
    assert(lastOpts.generation.logprobs.has_value());
    assert(*lastOpts.generation.logprobs == true);
    
    assert(lastOpts.generation.top_logprobs.has_value());
    assert(*lastOpts.generation.top_logprobs == 5);

    // Note: Parameter merging happens in ChatCompletionRequestHelper which FakeAPIClient bypasses.
    // So we verify proper propagation of options here. Implementation correctness is guaranteed 
    // by the shared helper code usage in real clients.

    std::cout << "PASS" << std::endl;
}

void test_cancellation() {
    std::cout << "Running test_cancellation..." << std::endl;

    auto clientPtr = std::make_unique<FakeAPIClient>();
    LLMEngine::LLMEngine engine(std::move(clientPtr), nlohmann::json{}, 24, false, nullptr);

    auto token = CancellationToken::create();
    RequestOptions options;
    options.cancellation_token = token;

    token->cancel();

    AnalysisInput input;
    input.withUserMessage("hi");

    // Just ensure it runs without crashing
    auto result = engine.analyze("hi", input.toJson(), "test", options);
    (void)result;
    
    std::cout << "PASS" << std::endl;
}

void test_multimodal() {
    std::cout << "Running test_multimodal..." << std::endl;
    AnalysisInput input;
    std::vector<AnalysisInput::ContentPart> parts;
    parts.push_back(AnalysisInput::ContentPart::createText("Look at this:"));
    parts.push_back(AnalysisInput::ContentPart::createImage("http://foo.bar/img.png"));
    input.withMessage("user", parts);

    nlohmann::json json = input.toJson();
    assert(json["messages"].size() == 1);
    auto& content = json["messages"][0]["content"];
    assert(content.is_array());
    assert(content.size() == 2);
    assert(content[0]["type"] == "text");
    assert(content[0]["text"] == "Look at this:");
    assert(content[1]["type"] == "image_url");
    assert(content[1]["image_url"]["url"] == "http://foo.bar/img.png");
    std::cout << "PASS" << std::endl;
}

int main() {
    try {
        test_analysis_input_messages();
        test_image_loading();
        test_request_options_injection();
        test_cancellation();
        test_multimodal();
        std::cout << "All new feature tests passed." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
