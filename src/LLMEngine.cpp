// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/LLMEngine.hpp"

#include "LLMEngine/APIClient.hpp"
#include "LLMEngine/Constants.hpp"
#include "LLMEngine/HttpStatus.hpp"
#include "LLMEngine/IConfigManager.hpp"
#include "LLMEngine/ITempDirProvider.hpp"
#include "LLMEngine/ProviderBootstrap.hpp"
#include "LLMEngine/RequestContextBuilder.hpp"
#include "LLMEngine/ResponseHandler.hpp"
#include "LLMEngine/TempDirectoryService.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>

// Namespace aliases to reduce verbosity
namespace LLM = ::LLMEngine;
namespace LLMAPI = ::LLMEngineAPI;
// Note: We keep the full LLMEngine::LLMEngine:: qualification for class methods
// to avoid ambiguity with the namespace name

// Helper function to initialize common defaults (logger and temp dir provider)
namespace {
void initializeDefaults(std::shared_ptr<LLM::Logger>& logger,
                        std::shared_ptr<LLM::ITempDirProvider>& temp_dir_provider,
                        const std::shared_ptr<LLM::ITempDirProvider>& provided_temp_dir_provider,
                        std::string& tmp_dir) {
    // Initialize logger if not already set
    if (!logger) {
        logger = std::make_shared<LLM::DefaultLogger>();
    }

    // Initialize temp dir provider if not provided
    if (!temp_dir_provider) {
        temp_dir_provider = provided_temp_dir_provider
                                ? provided_temp_dir_provider
                                : std::make_shared<LLM::DefaultTempDirProvider>();
    }

    // Set tmp_dir from provider if not already set
    if (tmp_dir.empty()) {
        tmp_dir = temp_dir_provider->getTempDir();
    }
}

// Helper function to parse LLMENGINE_DISABLE_DEBUG_FILES environment variable
// Returns true if debug files should be disabled, false otherwise.
// Parses common boolean representations: "0", "false", "False", "FALSE" enable debug files.
// Any other non-empty value disables debug files. Empty/unset enables debug files.
bool parseDisableDebugFilesEnv() {
    const char* env_value = std::getenv("LLMENGINE_DISABLE_DEBUG_FILES");
    if (env_value == nullptr) {
        return false; // Variable not set, enable debug files
    }

    // Empty string means enable debug files
    if (env_value[0] == '\0') {
        return false;
    }

    // Check for explicit "false" values (case-insensitive)
    std::string value(env_value);
    // Convert to lowercase for comparison
    std::transform(
        value.begin(), value.end(), value.begin(), [](unsigned char c) { return std::tolower(c); });

    if (value == "0" || value == "false" || value == "no" || value == "off") {
        return false; // Explicitly enable debug files
    }

    // Any other value (including "1", "true", "yes", "on") disables debug files
    return true;
}
} // namespace

// Dependency injection constructor
::LLMEngine::LLMEngine::LLMEngine(std::unique_ptr<LLMAPI::APIClient> client,
                                  const nlohmann::json& model_params,
                                  int log_retention_hours,
                                  bool debug,
                                  const std::shared_ptr<LLM::ITempDirProvider>& temp_dir_provider)
    : model_params_(model_params), log_retention_hours_(log_retention_hours), debug_(debug),
      temp_dir_provider_(nullptr), api_client_(std::move(client)),
      disable_debug_files_env_cached_(parseDisableDebugFilesEnv()) {
    initializeDefaults(logger_, temp_dir_provider_, temp_dir_provider, tmp_dir_);
    if (!api_client_) {
        throw std::runtime_error("API client must not be null");
    }
    provider_type_ = api_client_->getProviderType();
}

// Constructor for API-based providers (direct ProviderType)
::LLMEngine::LLMEngine::LLMEngine(LLMAPI::ProviderType provider_type,
                                  std::string_view api_key,
                                  std::string_view model,
                                  const nlohmann::json& model_params,
                                  int log_retention_hours,
                                  bool debug)
    : model_params_(model_params), log_retention_hours_(log_retention_hours), debug_(debug),
      temp_dir_provider_(nullptr), provider_type_(provider_type),
      disable_debug_files_env_cached_(parseDisableDebugFilesEnv()) {
    initializeDefaults(logger_, temp_dir_provider_, nullptr, tmp_dir_);

    // Resolve API key using ProviderBootstrap (respects environment variables)
    api_key_ = ProviderBootstrap::resolveApiKey(provider_type, api_key, "", logger_.get());
    model_ = std::string(model);

    // Store config manager for downstream factories
    config_manager_ = std::shared_ptr<LLMAPI::IConfigManager>(
        &LLMAPI::APIConfigManager::getInstance(), [](LLMAPI::IConfigManager*) {});

    initializeAPIClient();
}

// Constructor using config file
::LLMEngine::LLMEngine::LLMEngine(std::string_view provider_name,
                                  std::string_view api_key,
                                  std::string_view model,
                                  const nlohmann::json& model_params,
                                  int log_retention_hours,
                                  bool debug,
                                  const std::shared_ptr<LLMAPI::IConfigManager>& config_manager)
    : model_params_(model_params), log_retention_hours_(log_retention_hours), debug_(debug),
      temp_dir_provider_(nullptr), disable_debug_files_env_cached_(parseDisableDebugFilesEnv()) {
    initializeDefaults(logger_, temp_dir_provider_, nullptr, tmp_dir_);

    // Use ProviderBootstrap to resolve provider configuration
    auto bootstrap_result =
        ProviderBootstrap::bootstrap(provider_name, api_key, model, config_manager, logger_.get());

    // Store resolved values
    provider_type_ = bootstrap_result.provider_type;
    api_key_ = bootstrap_result.api_key;
    model_ = bootstrap_result.model;
    ollama_url_ = bootstrap_result.ollama_url;

    // Store config manager for downstream factories
    if (config_manager) {
        config_manager_ = config_manager;
    } else {
        // Wrap singleton in shared_ptr with no-op deleter
        config_manager_ = std::shared_ptr<LLMAPI::IConfigManager>(
            &LLMAPI::APIConfigManager::getInstance(), [](LLMAPI::IConfigManager*) {});
    }

    initializeAPIClient();
}

void ::LLMEngine::LLMEngine::initializeAPIClient() {
    // SECURITY: Validate API key for cloud providers before instantiating HTTP client
    // This fails fast during setup instead of deferring credential bugs to runtime 401s
    if (provider_type_ != LLMAPI::ProviderType::OLLAMA) {
        if (api_key_.empty()) {
            std::string env_var_name = ProviderBootstrap::getApiKeyEnvVarName(provider_type_);
            std::string error_msg = "No API key found for provider "
                                    + LLMAPI::APIClientFactory::providerTypeToString(provider_type_)
                                    + ". " + "Set the " + env_var_name
                                    + " environment variable or provide it in the constructor.";
            if (logger_) {
                logger_->log(LLM::LogLevel::Error, error_msg);
            }
            throw std::runtime_error(error_msg);
        }
    }

    if (provider_type_ == LLMAPI::ProviderType::OLLAMA) {
        api_client_ = LLMAPI::APIClientFactory::createClient(
            provider_type_, "", model_, ollama_url_, config_manager_);
    } else {
        api_client_ = LLMAPI::APIClientFactory::createClient(
            provider_type_, api_key_, model_, "", config_manager_);
    }

    if (!api_client_) {
        std::string provider_name = LLMAPI::APIClientFactory::providerTypeToString(provider_type_);
        throw std::runtime_error("Failed to create API client for provider: " + provider_name
                                 + ". Check that the provider type is valid and all required "
                                   "dependencies are available.");
    }
}

// Redaction and debug file I/O moved to DebugArtifacts

void ::LLMEngine::LLMEngine::ensureSecureTmpDir() {
    // Cache directory existence check to reduce filesystem operations
    // Only re-check if directory was previously verified and might have been deleted
    if (tmp_dir_verified_) {
        // Quick check: if directory still exists and is valid, skip expensive operations
        if (TempDirectoryService::isDirectoryValid(tmp_dir_, logger_.get())) {
            return;
        }
        // Directory was deleted or invalid, need to re-verify
        tmp_dir_verified_ = false;
    }

    // Use TempDirectoryService to ensure directory with security checks
    auto result = TempDirectoryService::ensureSecureDirectory(tmp_dir_, logger_.get());
    if (!result.success) {
        throw std::runtime_error(result.error_message);
    }

    // Mark as verified after successful creation/verification
    tmp_dir_verified_ = true;
}

LLM::AnalysisResult LLMEngine::LLMEngine::analyze(std::string_view prompt,
                                                  const nlohmann::json& input,
                                                  std::string_view analysis_type,
                                                  std::string_view mode,
                                                  bool prepend_terse_instruction) {
    // Ensure temp directory is prepared before building context (RAII enforcement)
    ensureSecureTmpDir();

    // Build request context (using IModelContext interface to break cyclic dependency)
    RequestContext ctx = RequestContextBuilder::build(static_cast<IModelContext&>(*this),
                                                      prompt,
                                                      input,
                                                      analysis_type,
                                                      mode,
                                                      prepend_terse_instruction);

    // Execute request via injected executor
    LLMAPI::APIResponse api_response;
    if (request_executor_) {
        api_response =
            request_executor_->execute(api_client_.get(), ctx.fullPrompt, input, ctx.finalParams);
    } else {
        if (logger_) {
            logger_->log(LLM::LogLevel::Error,
                         "Request executor not configured. This is an internal error - the API "
                         "client was not properly initialized.");
        }
        LLM::AnalysisResult result{
            .success = false,
            .think = "",
            .content = "",
            .errorMessage =
                "Request executor not configured. This is an internal error - the API client was "
                "not properly initialized. Please check your LLMEngine configuration.",
            .statusCode = HttpStatus::INTERNAL_SERVER_ERROR,
            .usage = {},
            .errorCode = LLMEngineErrorCode::Unknown};
        return result;
    }

    // Delegate response handling
    return ResponseHandler::handle(api_response,
                                   ctx.debugManager.get(),
                                   ctx.requestTmpDir,
                                   analysis_type,
                                   ctx.writeDebugFiles,
                                   logger_.get());
}

std::future<LLM::AnalysisResult> LLMEngine::LLMEngine::analyzeAsync(
    std::string_view prompt,
    const nlohmann::json& input,
    std::string_view analysis_type,
    std::string_view mode,
    bool prepend_terse_instruction) {
    // Capture arguments by value to ensure valid lifetime during async execution
    return std::async(std::launch::async,
                      [this,
                       prompt = std::string(prompt),
                       input = input,
                       analysis_type = std::string(analysis_type),
                       mode = std::string(mode),
                       prepend_terse_instruction]() {
                          return this->analyze(
                              prompt, input, analysis_type, mode, prepend_terse_instruction);
                      });
}

void LLMEngine::LLMEngine::analyzeStream(std::string_view prompt,
                                         const nlohmann::json& input,
                                         std::string_view analysis_type,
                                         std::string_view mode,
                                         bool prepend_terse_instruction,
                                         std::function<void(std::string_view, bool)> callback) {
    // Ensure temp directory is prepared
    ensureSecureTmpDir();

    // Build request context
    RequestContext ctx = RequestContextBuilder::build(static_cast<IModelContext&>(*this),
                                                      prompt,
                                                      input,
                                                      analysis_type,
                                                      mode,
                                                      prepend_terse_instruction);

    // Adapt callback for APIClient (which might not send is_done boolean, or we handle it)
    // The APIClient interface has: function<void(string_view)>
    // LLMEngine interface has: function<void(string_view, bool)>
    // We'll wrap it.
    auto wrapped_callback = [callback](std::string_view chunk) {
        // Assume APIClient implies not-done until it finishes?
        // Actually APIClient interface is synchronous-like or blocks until done?
        // sendRequestStream is void, so it blocks?
        // If it blocks, we invoke callback with chunk, false.
        // And when it returns, does it mean done?
        callback(chunk, false);
    };

    if (api_client_) {
        // TODO: Handle is_done properly. Currently APIClient assumes blocking stream?
        api_client_->sendRequestStream(ctx.fullPrompt, input, ctx.finalParams, wrapped_callback);
        // After return, send empty chunk with is_done=true?
        callback("", true);
    } else {
        if (logger_) {
            logger_->log(LLM::LogLevel::Error, "API client not initialized for streaming.");
        }
        // Signal error/completion?
        // For now just finish
        callback("", true);
    }
}

std::string LLMEngine::LLMEngine::getProviderName() const {
    if (api_client_) {
        return api_client_->getProviderName();
    }
    return "Ollama (Legacy)";
}

std::string LLMEngine::LLMEngine::getModelName() const {
    return model_;
}

LLMAPI::ProviderType LLMEngine::LLMEngine::getProviderType() const {
    return provider_type_;
}

bool ::LLMEngine::LLMEngine::isOnlineProvider() const {
    return provider_type_ != LLMAPI::ProviderType::OLLAMA;
}

void ::LLMEngine::LLMEngine::setLogger(std::shared_ptr<LLM::Logger> logger) {
    if (logger) {
        logger_ = std::move(logger);
    }
}

bool ::LLMEngine::LLMEngine::setTempDirectory(const std::string& tmp_dir) {
    // Only accept directories within the active provider's root to respect dependency injection
    const std::string allowed_root = temp_dir_provider_
                                         ? temp_dir_provider_->getTempDir()
                                         : LLM::DefaultTempDirProvider().getTempDir();

    if (TempDirectoryService::validatePathWithinRoot(tmp_dir, allowed_root, logger_.get())) {
        tmp_dir_ = tmp_dir;
        tmp_dir_verified_ = false; // Reset cache when directory changes
        return true;
    }
    return false;
}

std::string LLMEngine::LLMEngine::getTempDirectory() const {
    return tmp_dir_;
}
