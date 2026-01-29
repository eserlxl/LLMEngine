// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "EngineState.hpp"

#include "LLMEngine/APIClient.hpp"
#include "LLMEngine/Constants.hpp"
#include "LLMEngine/ProviderBootstrap.hpp"
#include "LLMEngine/TempDirectoryService.hpp"
#include "LLMEngine/Utils/Semaphore.hpp"

#include "LLMEngine/IArtifactSink.hpp"
#include "LLMEngine/PromptBuilder.hpp"
#include "LLMEngine/IRequestExecutor.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>

// Helper namespace
namespace {
bool parseDisableDebugFilesEnv() {
    const char* env_value = std::getenv("LLMENGINE_DISABLE_DEBUG_FILES");
    if (env_value == nullptr) {
        return false; // Variable not set, enable debug files
    }
    if (env_value[0] == '\0') {
        return false;
    }
    std::string value(env_value);
    std::transform(
        value.begin(), value.end(), value.begin(), [](unsigned char c) { return std::tolower(c); });

    if (value == "0" || value == "false" || value == "no" || value == "off") {
        return false;
    }
    return true;
}
} // namespace

namespace LLMEngine {

using namespace LLMEngineAPI; // For APIClient, ProviderType

// Note: LLMEngine here refers to the CLASS inside namespace LLMEngine.
// So LLMEngine::EngineState refers to the nested struct.

LLMEngine::EngineState::EngineState(const nlohmann::json& params, int cleanup_hours, bool debug)
    : modelParams_(params), logRetentionHours_(cleanup_hours), debug_(debug),
      apiKey_(""),
      disableDebugFilesEnvCached_(parseDisableDebugFilesEnv()) {
    // Initialize defaults
    logger_ = std::make_shared<DefaultLogger>();
    tempDirProvider_ = std::make_shared<DefaultTempDirProvider>();
    tmp_dir_ = tempDirProvider_->getTempDir();
    
    tersePromptBuilder_ = std::make_shared<TersePromptBuilder>();
    passthroughPromptBuilder_ = std::make_shared<PassthroughPromptBuilder>();
    requestExecutor_ = std::make_shared<DefaultRequestExecutor>();
    artifactSink_ = std::make_shared<DefaultArtifactSink>();
}

void LLMEngine::EngineState::initializeAPIClient() {
    if (providerType_ != ProviderType::OLLAMA) {
        if (apiKey_.empty()) {
            std::string env_var_name = ProviderBootstrap::getApiKeyEnvVarName(providerType_);
            std::string error_msg =
                "No API key found for provider "
                + APIClientFactory::providerTypeToString(providerType_) + ". Set the "
                + env_var_name + " environment variable or provide it in the constructor.";
            if (logger_) {
                logger_->log(LogLevel::Error, error_msg);
            }
            throw std::runtime_error(error_msg);
        }
    }

    if (providerType_ == ProviderType::OLLAMA) {
        apiClient_ = APIClientFactory::createClient(
            providerType_, "", model_, ollamaUrl_, configManager_);
    } else {
        apiClient_ = APIClientFactory::createClient(
            providerType_, apiKey_.view(), model_, "", configManager_);
    }

    if (!apiClient_) {
        std::string provider_name =
            APIClientFactory::providerTypeToString(providerType_);
        throw std::runtime_error("Failed to create API client: " + provider_name);
    }
}

void LLMEngine::EngineState::ensureSecureTmpDir() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (tempDirVerified_) {
        if (TempDirectoryService::isDirectoryValid(tmp_dir_, logger_.get())) {
            return;
        }
        tempDirVerified_ = false;
    }

    auto result = TempDirectoryService::ensureSecureDirectory(tmp_dir_, logger_.get());
    if (!result.success) {
        throw std::runtime_error(result.error_message);
    }
    tempDirVerified_ = true;
}

} // namespace LLMEngine
