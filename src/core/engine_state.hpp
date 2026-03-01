// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "llmengine/core/llm_engine.hpp"

#include "llmengine/providers/api_client.hpp"
#include "llmengine/diagnostics/i_artifact_sink.hpp"
#include "llmengine/core/i_config_manager.hpp"
#include "llmengine/diagnostics/i_metrics_collector.hpp"
#include "llmengine/core/prompt_builder.hpp"
#include "llmengine/providers/i_request_executor.hpp"
#include "llmengine/utils/i_temp_dir_provider.hpp"
#include "llmengine/utils/logger.hpp"
#include "llmengine/utils/secure_string.hpp"

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
    nlohmann::json modelParams_;
    int logRetentionHours_;
    bool debug_;
    std::string tmp_dir_;
    std::shared_ptr<ITempDirProvider> tempDirProvider_;
    bool tempDirVerified_ = false;

    std::unique_ptr<LLMEngineAPI::APIClient> apiClient_;
    std::shared_ptr<LLMEngineAPI::IConfigManager> configManager_;

    std::shared_ptr<IPromptBuilder> tersePromptBuilder_;
    std::shared_ptr<IPromptBuilder> passthroughPromptBuilder_;
    std::shared_ptr<IRequestExecutor> requestExecutor_;
    std::shared_ptr<IArtifactSink> artifactSink_;

    LLMEngineAPI::ProviderType providerType_;
    SecureString apiKey_;
    std::string ollamaUrl_;
    std::shared_ptr<Logger> logger_;
    std::shared_ptr<IMetricsCollector> metricsCollector_;
    std::function<bool()> debugFilesPolicy_;
    bool disableDebugFilesEnvCached_;
    std::vector<std::shared_ptr<IInterceptor>> interceptors_;
    std::mutex state_mutex_; // Protects ensuring secure directories
    mutable std::shared_mutex configMutex_; // Protects dynamic configuration (interceptors, settings)
    RequestOptions defaultRequestOptions_; // Default options applied to all requests

    EngineState(const nlohmann::json& params, int cleanup_hours, bool debug);

    void initializeAPIClient();
    void ensureSecureTmpDir();
};

} // namespace LLMEngine
