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
    : model_params_(params), log_retention_hours_(cleanup_hours), debug_(debug),
      api_key_(""),
      disable_debug_files_env_cached_(parseDisableDebugFilesEnv()) {
    // Initialize defaults
    logger_ = std::make_shared<DefaultLogger>();
    temp_dir_provider_ = std::make_shared<DefaultTempDirProvider>();
    tmp_dir_ = temp_dir_provider_->getTempDir();
    
    terse_prompt_builder_ = std::make_shared<TersePromptBuilder>();
    passthrough_prompt_builder_ = std::make_shared<PassthroughPromptBuilder>();
    request_executor_ = std::make_shared<DefaultRequestExecutor>();
    artifact_sink_ = std::make_shared<DefaultArtifactSink>();
}

void LLMEngine::EngineState::initializeAPIClient() {
    if (provider_type_ != ProviderType::OLLAMA) {
        if (api_key_.empty()) {
            std::string env_var_name = ProviderBootstrap::getApiKeyEnvVarName(provider_type_);
            std::string error_msg =
                "No API key found for provider "
                + APIClientFactory::providerTypeToString(provider_type_) + ". Set the "
                + env_var_name + " environment variable or provide it in the constructor.";
            if (logger_) {
                logger_->log(LogLevel::Error, error_msg);
            }
            throw std::runtime_error(error_msg);
        }
    }

    if (provider_type_ == ProviderType::OLLAMA) {
        api_client_ = APIClientFactory::createClient(
            provider_type_, "", model_, ollama_url_, config_manager_);
    } else {
        api_client_ = APIClientFactory::createClient(
            provider_type_, api_key_.view(), model_, "", config_manager_);
    }

    if (!api_client_) {
        std::string provider_name =
            APIClientFactory::providerTypeToString(provider_type_);
        throw std::runtime_error("Failed to create API client: " + provider_name);
    }
}

void LLMEngine::EngineState::ensureSecureTmpDir() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (tmp_dir_verified_) {
        if (TempDirectoryService::isDirectoryValid(tmp_dir_, logger_.get())) {
            return;
        }
        tmp_dir_verified_ = false;
    }

    auto result = TempDirectoryService::ensureSecureDirectory(tmp_dir_, logger_.get());
    if (!result.success) {
        throw std::runtime_error(result.error_message);
    }
    tmp_dir_verified_ = true;
}

} // namespace LLMEngine
