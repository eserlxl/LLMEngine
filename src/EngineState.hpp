// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "LLMEngine/LLMEngine.hpp"

#include "LLMEngine/APIClient.hpp"
#include "LLMEngine/IArtifactSink.hpp"
#include "LLMEngine/IConfigManager.hpp"
#include "LLMEngine/IMetricsCollector.hpp"
#include "LLMEngine/PromptBuilder.hpp"
#include "LLMEngine/IRequestExecutor.hpp"
#include "LLMEngine/ITempDirProvider.hpp"
#include "LLMEngine/Logger.hpp"
#include "LLMEngine/SecureString.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace LLMEngine {

struct LLMEngine::EngineState {
    std::string model_;
    nlohmann::json model_params_;
    int log_retention_hours_;
    bool debug_;
    std::string tmp_dir_;
    std::shared_ptr<ITempDirProvider> temp_dir_provider_;
    bool tmp_dir_verified_ = false;

    std::unique_ptr<LLMEngineAPI::APIClient> api_client_;
    std::shared_ptr<LLMEngineAPI::IConfigManager> config_manager_;

    std::shared_ptr<IPromptBuilder> terse_prompt_builder_;
    std::shared_ptr<IPromptBuilder> passthrough_prompt_builder_;
    std::shared_ptr<IRequestExecutor> request_executor_;
    std::shared_ptr<IArtifactSink> artifact_sink_;

    LLMEngineAPI::ProviderType provider_type_;
    SecureString api_key_;
    std::string ollama_url_;
    std::shared_ptr<Logger> logger_;
    std::shared_ptr<IMetricsCollector> metrics_collector_;
    std::function<bool()> debug_files_policy_;
    bool disable_debug_files_env_cached_;
    std::vector<std::shared_ptr<IInterceptor>> interceptors_;
    std::mutex state_mutex_; // Protects ensuring secure directories
    mutable std::shared_mutex config_mutex_; // Protects dynamic configuration (interceptors, settings)
    RequestOptions default_request_options_; // Default options applied to all requests

    EngineState(const nlohmann::json& params, int cleanup_hours, bool debug);

    void initializeAPIClient();
    void ensureSecureTmpDir();
};

} // namespace LLMEngine
