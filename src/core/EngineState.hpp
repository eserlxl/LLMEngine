// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "LLMEngine/core/LLMEngine.hpp"

#include "LLMEngine/providers/APIClient.hpp"
#include "LLMEngine/diagnostics/IArtifactSink.hpp"
#include "LLMEngine/core/IConfigManager.hpp"
#include "LLMEngine/diagnostics/IMetricsCollector.hpp"
#include "LLMEngine/core/PromptBuilder.hpp"
#include "LLMEngine/providers/IRequestExecutor.hpp"
#include "LLMEngine/utils/ITempDirProvider.hpp"
#include "LLMEngine/utils/Logger.hpp"
#include "LLMEngine/utils/SecureString.hpp"

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
