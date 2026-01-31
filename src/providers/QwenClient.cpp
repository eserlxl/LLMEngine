// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/providers/APIClient.hpp"
#include "LLMEngine/core/Constants.hpp"
#include "OpenAICompatibleClient.hpp"

#include <memory>
#include <nlohmann/json.hpp>

namespace LLMEngineAPI {

// PIMPL implementation - OpenAICompatibleClient is not an APIClient, just a helper
class QwenClient::Impl : public OpenAICompatibleClient {
  public:
    Impl(const std::string& api_key, const std::string& model)
        : OpenAICompatibleClient(
              api_key, model, std::string(::LLMEngine::Constants::DefaultUrls::QWEN_BASE)) {}
};

QwenClient::QwenClient(const std::string& api_key, const std::string& model)
    : impl_(std::make_unique<Impl>(api_key, model)) {}

QwenClient::~QwenClient() = default;

nlohmann::json QwenClient::buildPayload(std::string_view prompt, const nlohmann::json& input, const nlohmann::json& request_params) const {
    nlohmann::json messages = nlohmann::json::array();
    if (input.contains("messages") && input["messages"].is_array()) {
        messages = input["messages"];
    }
    if (!prompt.empty()) {
        messages.push_back({{"role", "user"}, {"content", prompt}});
    }
    return impl_->buildPayload(messages, request_params);
}

std::string QwenClient::buildUrl() const {
    return impl_->buildUrl();
}

std::map<std::string, std::string> QwenClient::buildHeaders() const {
    return impl_->getHeaders();
}

APIResponse QwenClient::sendRequest(std::string_view prompt,
                                    const nlohmann::json& input,
                                    const nlohmann::json& params,
                                    const ::LLMEngine::RequestOptions& options) const {
    return ChatCompletionRequestHelper::execute(
        impl_->getDefaultParams(),
        params,
        [&](const nlohmann::json& requestParams) {
             return buildPayload(prompt, input, requestParams);
        },
        [&]() { return buildUrl(); },
        [&]() { return buildHeaders(); },
        [](APIResponse& response, const std::string& raw_text) {
             OpenAICompatibleClient::parseOpenAIResponse(response, raw_text);
        },
        options,
        /*exponential_retry=*/true,
        impl_->getConfig()
    );
}

void QwenClient::sendRequestStream(std::string_view prompt,
                                     const nlohmann::json& input,
                                     const nlohmann::json& params,
                                     LLMEngine::StreamCallback callback,
                                     const ::LLMEngine::RequestOptions& options) const {
    auto buffer = std::make_shared<std::string>();

    ChatCompletionRequestHelper::executeStream(
        impl_->getDefaultParams(),
        params,
        [&](const nlohmann::json& requestParams) {
            return buildPayload(prompt, input, requestParams);
        },
        [&]() { return buildUrl(); },
        [&]() { return buildHeaders(); },
        [buffer, callback](std::string_view chunk) {
            OpenAICompatibleClient::parseOpenAIStreamChunk(chunk, *buffer, callback);
        },
        options,
        /*exponential_retry=*/true,
        impl_->getConfig());

    // Send final done signal
    LLMEngine::StreamChunk result;
    result.is_done = true;
    callback(result);
}

void QwenClient::setConfig(std::shared_ptr<IConfigManager> cfg) {
    impl_->setConfig(std::move(cfg));
}

} // namespace LLMEngineAPI
