// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "ChatCompletionRequestHelper.hpp"
#include "LLMEngine/APIClient.hpp"
#include "LLMEngine/Constants.hpp"
#include "OpenAICompatibleClient.hpp"

#include <memory>
#include <nlohmann/json.hpp>

namespace LLMEngineAPI {

// PIMPL implementation - OpenAICompatibleClient is not an APIClient, just a helper
class OpenAIClient::Impl : public OpenAICompatibleClient {
  public:
    Impl(const std::string& api_key, const std::string& model)
        : OpenAICompatibleClient(
              api_key, model, std::string(::LLMEngine::Constants::DefaultUrls::OPENAI_BASE)) {}
};

OpenAIClient::OpenAIClient(const std::string& api_key, const std::string& model)
    : impl_(std::make_unique<Impl>(api_key, model)) {}

OpenAIClient::~OpenAIClient() = default;

APIResponse OpenAIClient::sendRequest(std::string_view prompt,
                                      const nlohmann::json& input,
                                      const nlohmann::json& params) const {
    // Build messages array using shared helper
    const nlohmann::json messages = ChatMessageBuilder::buildMessages(prompt, input);

    // Use shared request helper for common lifecycle
    return ChatCompletionRequestHelper::execute(
        impl_->getDefaultParams(),
        params,
        // Build payload using base class method
        [&](const nlohmann::json& request_params) {
            return impl_->buildPayload(messages, request_params);
        },
        // Build URL using base class method
        [&]() { return impl_->buildUrl(); },
        // Build headers using base class method (returns cached headers)
        [&]() { return impl_->getHeaders(); },
        // Parse response using base class method
        OpenAICompatibleClient::parseOpenAIResponse,
        /*exponential_retry*/ true,
        impl_->getConfig());
}

void OpenAIClient::sendRequestStream(std::string_view prompt,
                                     const nlohmann::json& input,
                                     const nlohmann::json& params,
                                     std::function<void(std::string_view)> callback) const {
    impl_->sendRequestStream(prompt, input, params, callback);
}

void OpenAIClient::setConfig(std::shared_ptr<IConfigManager> cfg) {
    impl_->setConfig(std::move(cfg));
}

} // namespace LLMEngineAPI
