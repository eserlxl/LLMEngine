// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of the LLMEngine test support (fake API client) and is
// licensed under the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "FakeAPIClient.hpp"

namespace LLMEngineAPI {

FakeAPIClient::FakeAPIClient(ProviderType type, std::string providerName)
    : provider_type_(type), provider_name_(std::move(providerName)) {}

void FakeAPIClient::setNextResponse(const APIResponse& response) {
    next_response_ = response;
    has_custom_response_ = true;
}

APIResponse FakeAPIClient::sendRequest(const std::string& prompt,
                                       const nlohmann::json& input,
                                       const nlohmann::json& params) const {
    if (has_custom_response_) {
        has_custom_response_ = false;
        return next_response_;
    }

    APIResponse r;
    r.success = true;
    // Produce a deterministic echo-style response for tests
    r.content = "[FAKE] " + prompt;
    r.status_code = 200;
    r.raw_response = {
        {"fake", true},
        {"provider", provider_name_},
        {"prompt_len", static_cast<int>(prompt.size())},
        {"has_system", input.contains("system_prompt")}
    };
    return r;
}

} // namespace LLMEngineAPI


