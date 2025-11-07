// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of the LLMEngine test support (fake API client) and is
// licensed under the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include "LLMEngine/APIClient.hpp"

namespace LLMEngineAPI {

class FakeAPIClient : public APIClient {
  public:
    explicit FakeAPIClient(ProviderType type = ProviderType::OPENAI,
                           std::string providerName = "Fake");

    APIResponse sendRequest(std::string_view prompt,
                            const nlohmann::json& input,
                            const nlohmann::json& params) const override;

    std::string getProviderName() const override {
        return provider_name_;
    }
    ProviderType getProviderType() const override {
        return provider_type_;
    }

    void setNextResponse(const APIResponse& response);

  private:
    ProviderType provider_type_;
    std::string provider_name_;
    mutable bool has_custom_response_ = false;
    mutable APIResponse next_response_{};
};

} // namespace LLMEngineAPI
