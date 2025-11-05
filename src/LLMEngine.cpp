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
#include <filesystem>
#include <cstdlib>
#include <stdexcept>
#include <system_error>

// (orchestration helpers removed; analyze now directly coordinates collaborators)

// Dependency injection constructor
LLMEngine::LLMEngine::LLMEngine(std::unique_ptr<::LLMEngineAPI::APIClient> client,
                     const nlohmann::json& model_params,
                     int log_retention_hours,
                     bool debug,
                     const std::shared_ptr<::LLMEngine::ITempDirProvider>& temp_dir_provider)
    : model_params_(model_params),
      log_retention_hours_(log_retention_hours),
      debug_(debug),
      tmp_dir_(temp_dir_provider ? temp_dir_provider->getTempDir() : ::LLMEngine::DefaultTempDirProvider().getTempDir()),
      temp_dir_provider_(temp_dir_provider ? temp_dir_provider : std::make_shared<::LLMEngine::DefaultTempDirProvider>()),
      api_client_(std::move(client)) {
    logger_ = std::make_shared<::LLMEngine::DefaultLogger>();
    if (!api_client_) {
        throw std::runtime_error("API client must not be null");
    }
    provider_type_ = api_client_->getProviderType();
}

// New constructor for API-based providers
LLMEngine::LLMEngine::LLMEngine(::LLMEngineAPI::ProviderType provider_type,
                     std::string_view api_key,
                     std::string_view model,
                     const nlohmann::json& model_params,
                     int log_retention_hours,
                     bool debug)
    : model_(std::string(model)),
      model_params_(model_params),
      log_retention_hours_(log_retention_hours),
      debug_(debug),
      tmp_dir_(::LLMEngine::DefaultTempDirProvider().getTempDir()),
      temp_dir_provider_(std::make_shared<::LLMEngine::DefaultTempDirProvider>()),
      provider_type_(provider_type),
      api_key_(std::string(api_key)) {
    logger_ = std::make_shared<::LLMEngine::DefaultLogger>();
    initializeAPIClient();
}

// Constructor using config file
LLMEngine::LLMEngine::LLMEngine(std::string_view provider_name,
                     std::string_view api_key,
                     std::string_view model,
                     const nlohmann::json& model_params,
                     int log_retention_hours,
                     bool debug,
                     const std::shared_ptr<::LLMEngineAPI::IConfigManager>& config_manager)
    : model_params_(model_params),
      log_retention_hours_(log_retention_hours),
      debug_(debug),
      tmp_dir_(::LLMEngine::DefaultTempDirProvider().getTempDir()),
      temp_dir_provider_(std::make_shared<::LLMEngine::DefaultTempDirProvider>()),
      api_key_(std::string(api_key)) {
    logger_ = std::make_shared<::LLMEngine::DefaultLogger>();
    
    // Use injected config manager or fall back to singleton
    std::shared_ptr<::LLMEngineAPI::IConfigManager> config_mgr = config_manager;
    if (!config_mgr) {
        // Wrap singleton in shared_ptr with no-op deleter for compatibility
        config_mgr = std::shared_ptr<::LLMEngineAPI::IConfigManager>(
            &::LLMEngineAPI::APIConfigManager::getInstance(),
            [](::LLMEngineAPI::IConfigManager*){});
    }
    // Store for downstream factories
    config_manager_ = config_mgr;
    
    // Load config
    if (!config_mgr->loadConfig()) {
        logger_->log(::LLMEngine::LogLevel::Warn, "Could not load API config, using defaults");
    }
    
    // Determine provider name (use default if empty)
    std::string resolved_provider(provider_name);
    if (resolved_provider.empty()) {
        resolved_provider = config_mgr->getDefaultProvider();
        if (resolved_provider.empty()) {
            resolved_provider = "ollama";
        }
    }

    // Get provider configuration
    auto provider_config = config_mgr->getProviderConfig(resolved_provider);
    if (provider_config.empty()) {
        logger_->log(::LLMEngine::LogLevel::Error, std::string("Provider ") + resolved_provider + " not found in config");
        throw std::runtime_error("Invalid provider name");
    }
    
    // Set provider type
    provider_type_ = ::LLMEngineAPI::APIClientFactory::stringToProviderType(resolved_provider);
    
    // SECURITY: Prefer environment variables for API keys over config file or constructor parameter
    // Only use provided api_key if environment variable is not set
    std::string env_var_name;
    switch (provider_type_) {
        case ::LLMEngineAPI::ProviderType::QWEN: env_var_name = "QWEN_API_KEY"; break;
        case ::LLMEngineAPI::ProviderType::OPENAI: env_var_name = "OPENAI_API_KEY"; break;
        case ::LLMEngineAPI::ProviderType::ANTHROPIC: env_var_name = "ANTHROPIC_API_KEY"; break;
        case ::LLMEngineAPI::ProviderType::GEMINI: env_var_name = "GEMINI_API_KEY"; break;
        default: break;
    }
    
    // Override API key with environment variable if available
    // SECURITY: Guard against calling std::getenv("") which is undefined behavior
    const char* env_api_key = nullptr;
    if (!env_var_name.empty()) {
        env_api_key = std::getenv(env_var_name.c_str());
    }
    if (env_api_key && strlen(env_api_key) > 0) {
        api_key_ = env_api_key;
    } else if (!std::string(api_key).empty()) {
        // Use provided API key if environment variable is not set
        api_key_ = std::string(api_key);
    } else {
        // Fall back to config file (last resort - not recommended for production)
        api_key_ = provider_config.value("api_key", "");
        if (!api_key_.empty()) {
            logger_->log(::LLMEngine::LogLevel::Warn, std::string("Using API key from config file. For production use, ")
                      + "set the " + env_var_name + " environment variable instead. "
                      + "Storing credentials in config files is a security risk.");
        }
    }
    
    // Set model
    if (std::string(model).empty()) {
        model_ = provider_config.value("default_model", "");
    } else {
        model_ = std::string(model);
    }
    
    // Set Ollama URL if using Ollama
    if (provider_type_ == ::LLMEngineAPI::ProviderType::OLLAMA) {
        ollama_url_ = provider_config.value("base_url", "http://localhost:11434");
    }
    
    initializeAPIClient();
}

void LLMEngine::LLMEngine::initializeAPIClient() {
    if (provider_type_ == ::LLMEngineAPI::ProviderType::OLLAMA) {
        api_client_ = ::LLMEngineAPI::APIClientFactory::createClient(
            provider_type_, "", model_, ollama_url_, config_manager_);
    } else {
        api_client_ = ::LLMEngineAPI::APIClientFactory::createClient(
            provider_type_, api_key_, model_, "", config_manager_);
    }
    
    if (!api_client_) {
        throw std::runtime_error("Failed to create API client");
    }
}

// Redaction and debug file I/O moved to DebugArtifacts

void LLMEngine::LLMEngine::ensureSecureTmpDir() const {
    // Security: Check for symlink before creating directories
    // If the path exists and is a symlink, reject it to prevent symlink traversal attacks
    std::error_code ec;
    if (std::filesystem::exists(tmp_dir_, ec)) {
        std::error_code ec_symlink;
        if (std::filesystem::is_symlink(tmp_dir_, ec_symlink)) {
            if (logger_) {
                logger_->log(::LLMEngine::LogLevel::Error, std::string("Temporary directory is a symlink: ") + tmp_dir_);
            }
            throw std::runtime_error("Temporary directory cannot be a symlink for security reasons: " + tmp_dir_);
        }
    }
    
    std::filesystem::create_directories(tmp_dir_, ec);
    if (ec) {
        if (logger_) {
            logger_->log(::LLMEngine::LogLevel::Error, std::string("Failed to create temporary directory: ") + tmp_dir_ + ": " + ec.message());
        }
        throw std::runtime_error("Failed to create temporary directory: " + tmp_dir_);
    }
    
    if (std::filesystem::exists(tmp_dir_)) {
        std::error_code ec_perm;
        std::filesystem::permissions(tmp_dir_, 
            std::filesystem::perms::owner_read | std::filesystem::perms::owner_write | std::filesystem::perms::owner_exec,
            std::filesystem::perm_options::replace, ec_perm);
        if (ec_perm && logger_) {
            logger_->log(::LLMEngine::LogLevel::Warn, std::string("Failed to set permissions on ") + tmp_dir_ + ": " + ec_perm.message());
        }
    }
}

// Removed legacy cleanup hook; per-request directories are managed by DebugArtifactManager


::LLMEngine::AnalysisResult LLMEngine::LLMEngine::analyze(std::string_view prompt, 
                                  const nlohmann::json& input, 
                                  std::string_view analysis_type, 
                                  std::string_view mode,
                                  bool prepend_terse_instruction) const {
    // Build request context
    RequestContext ctx = RequestContextBuilder::build(*this, prompt, input, analysis_type, mode, prepend_terse_instruction);

    // Execute request via injected executor
    ::LLMEngineAPI::APIResponse api_response;
    if (request_executor_) {
        api_response = request_executor_->execute(api_client_.get(), ctx.fullPrompt, input, ctx.finalParams);
    } else {
        if (logger_) {
            logger_->log(::LLMEngine::LogLevel::Error, "Request executor not configured");
        }
        ::LLMEngine::AnalysisResult result{false, "", "", "Request executor not configured", HttpStatus::INTERNAL_SERVER_ERROR};
        result.errorCode = AnalysisErrorCode::Unknown;
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

::LLMEngineAPI::ProviderType LLMEngine::LLMEngine::getProviderType() const {
    return provider_type_;
}

bool LLMEngine::LLMEngine::isOnlineProvider() const {
    return provider_type_ != ::LLMEngineAPI::ProviderType::OLLAMA;
}

void LLMEngine::LLMEngine::setLogger(std::shared_ptr<::LLMEngine::Logger> logger) {
    if (logger) {
        logger_ = std::move(logger);
    }
}

bool LLMEngine::LLMEngine::setTempDirectory(const std::string& tmp_dir) {
    // Only accept directories within the active provider's root to respect dependency injection
    try {
        const std::string allowed_root = temp_dir_provider_ ? temp_dir_provider_->getTempDir() : ::LLMEngine::DefaultTempDirProvider().getTempDir();
        const std::filesystem::path default_root = std::filesystem::path(allowed_root).lexically_normal();
        const std::filesystem::path requested    = std::filesystem::path(tmp_dir).lexically_normal();
        // Ensure requested is under default_root using a robust relative check
        const std::filesystem::path rel = std::filesystem::weakly_canonical(requested).lexically_relative(std::filesystem::weakly_canonical(default_root));
        bool has_parent_ref = false;
        for (const auto& part : rel) {
            if (part == "..") { has_parent_ref = true; break; }
        }
        const bool is_within = !rel.empty() && !rel.is_absolute() && !has_parent_ref;
        if (is_within) {
            tmp_dir_ = requested.string();
            return true;
        }
        if (logger_) {
            logger_->log(::LLMEngine::LogLevel::Warn, std::string("Rejected temp directory outside allowed root: ") + requested.string());
        }
        return false;
    } catch (...) {
        if (logger_) {
            logger_->log(::LLMEngine::LogLevel::Error, std::string("Failed to set temp directory due to path error: ") + tmp_dir);
        }
        return false;
    }
}

std::string LLMEngine::LLMEngine::getTempDirectory() const {
    return tmp_dir_;
}

