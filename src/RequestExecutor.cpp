// Copyright © 2026 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LLMEngine/APIClient.hpp"
#include "LLMEngine/IRequestExecutor.hpp"

namespace LLMEngine {

namespace {
constexpr int httpStatusInternalServerError = 500;
}

::LLMEngineAPI::APIResponse DefaultRequestExecutor::execute(
    const ::LLMEngineAPI::APIClient* api_client,
    const std::string& full_prompt,
    const nlohmann::json& input,
    const nlohmann::json& final_params,
    const ::LLMEngine::RequestOptions& options) const {
    if (!api_client) {
        ::LLMEngineAPI::APIResponse error_response;
        error_response.success = false;
        error_response.error_message = "API client not initialized";
        error_response.status_code = httpStatusInternalServerError;
        error_response.error_code = LLMEngine::LLMEngineErrorCode::Unknown;
        return error_response;
    }
    return api_client->sendRequest(full_prompt, input, final_params, options);
}

void DefaultRequestExecutor::executeStream(const ::LLMEngineAPI::APIClient* api_client,
                                           const std::string& full_prompt,
                                           const nlohmann::json& input,
                                           const nlohmann::json& final_params,
                                           LLMEngine::StreamCallback callback,
                                           const ::LLMEngine::RequestOptions& options) const {
    if (!api_client) {
        throw std::runtime_error("API client not initialized");
    }
    api_client->sendRequestStream(full_prompt, input, final_params, std::move(callback), options);
}

} // namespace LLMEngine
