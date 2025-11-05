// Copyright © 2025 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LLMEngine/IRequestExecutor.hpp"
#include "LLMEngine/APIClient.hpp"

namespace LLMEngine {

::LLMEngineAPI::APIResponse DefaultRequestExecutor::execute(
    const ::LLMEngineAPI::APIClient* api_client,
    const std::string& full_prompt,
    const nlohmann::json& input,
    const nlohmann::json& final_params) const {
    if (!api_client) {
        ::LLMEngineAPI::APIResponse error_response;
        error_response.success = false;
        error_response.error_message = "API client not initialized";
        error_response.status_code = 500;
        error_response.error_code = ::LLMEngineAPI::APIResponse::APIError::Unknown;
        return error_response;
    }
    return api_client->sendRequest(full_prompt, input, final_params);
}

} // namespace LLMEngine


