// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include "LLMEngine/LLMEngineExport.hpp"
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

namespace LLMEngineAPI {
class APIClient;
struct APIResponse;
} // namespace LLMEngineAPI

namespace LLMEngine {

/**
 * @brief Interface for executing provider requests.
 */
class LLMENGINE_EXPORT IRequestExecutor {
public:
    virtual ~IRequestExecutor() = default;

    /**
     * @brief Execute a request using the provided API client.
     */
    [[nodiscard]] virtual ::LLMEngineAPI::APIResponse execute(
        const ::LLMEngineAPI::APIClient* api_client, const std::string& full_prompt,
        const nlohmann::json& input, const nlohmann::json& final_params) const = 0;
};

/**
 * @brief Default request executor that directly calls APIClient::sendRequest.
 */
class LLMENGINE_EXPORT DefaultRequestExecutor : public IRequestExecutor {
public:
    [[nodiscard]] ::LLMEngineAPI::APIResponse execute(
        const ::LLMEngineAPI::APIClient* api_client, const std::string& full_prompt,
        const nlohmann::json& input, const nlohmann::json& final_params) const override;
};

} // namespace LLMEngine
