// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "APIClient.hpp"

namespace LLMEngineAPI {

class FakeAPIClient : public APIClient {
public:
    explicit FakeAPIClient(ProviderType type = ProviderType::OPENAI,
                           std::string providerName = "Fake");

    APIResponse sendRequest(const std::string& prompt,
                            const nlohmann::json& input,
                            const nlohmann::json& params) const override;

    std::string getProviderName() const override { return provider_name_; }
    ProviderType getProviderType() const override { return provider_type_; }

    void setNextResponse(const APIResponse& response);

private:
    ProviderType provider_type_;
    std::string provider_name_;
    mutable bool has_custom_response_ = false;
    mutable APIResponse next_response_{};
};

} // namespace LLMEngineAPI


