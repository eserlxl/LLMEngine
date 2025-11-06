// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/LLMEngine.hpp"
#include "LLMEngine/ITempDirProvider.hpp"
#include "LLMEngine/Constants.hpp"
#include "LLMEngine/HttpStatus.hpp"
#include "LLMEngine/IConfigManager.hpp"
#include "LLMEngine/APIClient.hpp"
#include "LLMEngine/RequestContextBuilder.hpp"
#include "LLMEngine/ResponseHandler.hpp"
#include "LLMEngine/ProviderBootstrap.hpp"
#include "LLMEngine/TempDirectoryService.hpp"
#include <filesystem>
#include <cstdlib>
#include <stdexcept>
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
            temp_dir_provider = provided_temp_dir_provider ? provided_temp_dir_provider : std::make_shared<LLM::DefaultTempDirProvider>();
        }
        
        // Set tmp_dir from provider if not already set
        if (tmp_dir.empty()) {
            tmp_dir = temp_dir_provider->getTempDir();
        }
    }
}

// Dependency injection constructor
::LLMEngine::LLMEngine::LLMEngine(std::unique_ptr<LLMAPI::APIClient> client,
                     const nlohmann::json& model_params,
                     int log_retention_hours,
                     bool debug,
                     const std::shared_ptr<LLM::ITempDirProvider>& temp_dir_provider)
    : model_params_(model_params),
      log_retention_hours_(log_retention_hours),
      debug_(debug),
      temp_dir_provider_(nullptr),
      api_client_(std::move(client)) {
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
    : model_params_(model_params),
      log_retention_hours_(log_retention_hours),
      debug_(debug),
      temp_dir_provider_(nullptr),
      provider_type_(provider_type) {
    initializeDefaults(logger_, temp_dir_provider_, nullptr, tmp_dir_);
    
    // Resolve API key using ProviderBootstrap (respects environment variables)
    api_key_ = ProviderBootstrap::resolveApiKey(provider_type, api_key, "", logger_.get());
    model_ = std::string(model);
    
    // Store config manager for downstream factories
    config_manager_ = std::shared_ptr<LLMAPI::IConfigManager>(
        &LLMAPI::APIConfigManager::getInstance(),
        [](LLMAPI::IConfigManager*){});
    
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
    : model_params_(model_params),
      log_retention_hours_(log_retention_hours),
      debug_(debug),
      temp_dir_provider_(nullptr) {
    initializeDefaults(logger_, temp_dir_provider_, nullptr, tmp_dir_);
    
    // Use ProviderBootstrap to resolve provider configuration
    auto bootstrap_result = ProviderBootstrap::bootstrap(
        provider_name,
        api_key,
        model,
        config_manager,
        logger_.get()
    );
    
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
            &LLMAPI::APIConfigManager::getInstance(),
            [](LLMAPI::IConfigManager*){});
    }
    
    initializeAPIClient();
}

void ::LLMEngine::LLMEngine::initializeAPIClient() {
    if (provider_type_ == LLMAPI::ProviderType::OLLAMA) {
        api_client_ = LLMAPI::APIClientFactory::createClient(
            provider_type_, "", model_, ollama_url_, config_manager_);
    } else {
        api_client_ = LLMAPI::APIClientFactory::createClient(
            provider_type_, api_key_, model_, "", config_manager_);
    }
    
    if (!api_client_) {
        throw std::runtime_error("Failed to create API client");
    }
}

// Redaction and debug file I/O moved to DebugArtifacts

void ::LLMEngine::LLMEngine::ensureSecureTmpDir() const {
    // Cache directory existence check to reduce filesystem operations
    // Only re-check if directory was previously verified and might have been deleted
    TempDirectoryService service;
    if (tmp_dir_verified_) {
        // Quick check: if directory still exists and is valid, skip expensive operations
        if (service.isDirectoryValid(tmp_dir_, logger_.get())) {
            return;
        }
        // Directory was deleted or invalid, need to re-verify
        tmp_dir_verified_ = false;
    }
    
    // Use TempDirectoryService to ensure directory with security checks
    auto result = service.ensureSecureDirectory(tmp_dir_, logger_.get());
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
                                  bool prepend_terse_instruction) const {
    // Ensure temp directory is prepared before building context (RAII enforcement)
    ensureSecureTmpDir();
    
    // Build request context (using IModelContext interface to break cyclic dependency)
    RequestContext ctx = RequestContextBuilder::build(static_cast<const IModelContext&>(*this), prompt, input, analysis_type, mode, prepend_terse_instruction);

    // Execute request via injected executor
    LLMAPI::APIResponse api_response;
    if (request_executor_) {
        api_response = request_executor_->execute(api_client_.get(), ctx.fullPrompt, input, ctx.finalParams);
    } else {
        if (logger_) {
            logger_->log(LLM::LogLevel::Error, "Request executor not configured");
        }
        LLM::AnalysisResult result{false, "", "", "Request executor not configured", HttpStatus::INTERNAL_SERVER_ERROR};
        result.errorCode = LLMEngineErrorCode::Unknown;
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
    const std::string allowed_root = temp_dir_provider_ ? temp_dir_provider_->getTempDir() : LLM::DefaultTempDirProvider().getTempDir();
    
    TempDirectoryService service;
    if (service.validatePathWithinRoot(tmp_dir, allowed_root, logger_.get())) {
        tmp_dir_ = tmp_dir;
        tmp_dir_verified_ = false;  // Reset cache when directory changes
        return true;
    }
    return false;
}

std::string LLMEngine::LLMEngine::getTempDirectory() const {
    return tmp_dir_;
}

