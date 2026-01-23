// Copyright © 2026 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LLMEngine/APIClient.hpp"
#include "LLMEngine/IRequestExecutor.hpp"

namespace LLMEngine {

namespace {
constexpr int kHttpStatusInternalServerError = 500;
}

::LLMEngineAPI::APIResponse DefaultRequestExecutor::execute(
    const ::LLMEngineAPI::APIClient* api_client,
    const std::string& full_prompt,
    const nlohmann::json& input,
    const nlohmann::json& final_params) const {
    if (!api_client) {
        ::LLMEngineAPI::APIResponse error_response;
        error_response.success = false;
        error_response.error_message = "API client not initialized";
        error_response.status_code = kHttpStatusInternalServerError;
        error_response.error_code = LLMEngine::LLMEngineErrorCode::Unknown;
        return error_response;
    }
    return api_client->sendRequest(full_prompt, input, final_params);
}

} // namespace LLMEngine
