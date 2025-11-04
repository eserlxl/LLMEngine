// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/LLMEngine.hpp"
#include "LLMEngine/ITempDirProvider.hpp"
#include "LLMEngine/PromptBuilder.hpp"
#include "LLMEngine/DebugArtifactManager.hpp"
#include "LLMEngine/Constants.hpp"
#include "LLMEngine/LLMOutputProcessor.hpp"
#include "LLMEngine/Utils.hpp"
#include "LLMEngine/DebugArtifacts.hpp"
#include "LLMEngine/IConfigManager.hpp"
#include "LLMEngine/APIClient.hpp"
#include "LLMEngine/ResponseParser.hpp"
#include "LLMEngine/ParameterMerger.hpp"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <ctime>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <mutex>
#include <thread>
#include <filesystem>
#include <cstdlib>

// Internal subwrapper namespace and Core orchestrator
namespace LLMEngineSystem {
    // Constants used by Core orchestration
    namespace {
        constexpr int HTTP_STATUS_OK = 200;
        constexpr int HTTP_STATUS_INTERNAL_SERVER_ERROR = 500;
    }
    struct Context {
        const nlohmann::json& model_params;
        int log_retention_hours;
        bool debug;
        const std::string& tmp_dir;
        const ::LLMEngineAPI::APIClient* api_client;
        ::LLMEngineAPI::ProviderType provider_type;
        const std::shared_ptr<::LLMEngine::Logger>& logger;
        const std::string& model;
    };

    // Collaborator: Prompt Strategy
    // Encapsulates prompt building logic to allow easy swapping of strategies
    inline std::string buildPrompt(std::string_view prompt, bool prepend_terse_instruction) {
        if (prepend_terse_instruction) {
            ::LLMEngine::TersePromptBuilder builder;
            return builder.buildPrompt(prompt);
        } else {
            ::LLMEngine::PassthroughPromptBuilder builder;
            return builder.buildPrompt(prompt);
        }
    }

    // Collaborator: Request Executor
    // Handles API request execution and response handling
    struct RequestExecutor {
        static ::LLMEngineAPI::APIResponse execute(
            const ::LLMEngineAPI::APIClient* api_client,
            const std::string& full_prompt,
            const nlohmann::json& input,
            const nlohmann::json& final_params) {
            if (!api_client) {
                ::LLMEngineAPI::APIResponse error_response;
                error_response.success = false;
                error_response.error_message = "API client not initialized";
                error_response.status_code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
                error_response.error_code = ::LLMEngineAPI::APIResponse::APIError::Unknown;
                return error_response;
            }
            return api_client->sendRequest(full_prompt, input, final_params);
        }
    };

    // Collaborator: Artifact Sink
    // Manages debug artifact creation and writing
    struct ArtifactSink {
        static std::unique_ptr<::LLMEngine::DebugArtifactManager> create(
            const std::string& request_tmp_dir,
            const std::string& base_tmp_dir,
            int log_retention_hours,
            ::LLMEngine::Logger* logger) {
            auto mgr = std::make_unique<::LLMEngine::DebugArtifactManager>(
                request_tmp_dir, base_tmp_dir, log_retention_hours, logger);
            mgr->ensureRequestDirectory();
            return mgr;
        }

        static void writeApiResponse(::LLMEngine::DebugArtifactManager* mgr,
                                    const ::LLMEngineAPI::APIResponse& response,
                                    bool is_error) {
            if (mgr) {
                mgr->writeApiResponse(response, is_error);
            }
        }

        static void writeFullResponse(::LLMEngine::DebugArtifactManager* mgr,
                                     std::string_view full_response) {
            if (mgr) {
                mgr->writeFullResponse(full_response);
            }
        }

        static void writeAnalysisArtifacts(::LLMEngine::DebugArtifactManager* mgr,
                                          std::string_view analysis_type,
                                          std::string_view think_section,
                                          std::string_view remaining_section) {
            if (mgr) {
                mgr->writeAnalysisArtifacts(analysis_type, think_section, remaining_section);
            }
        }
    };

    // Free function instead of class with static method to avoid unnecessary allocation
    // Use templates for callable parameters to avoid std::function heap allocations
    template <typename CleanupFunc, typename EnsureSecureFunc, typename GetUniqueTmpDirFunc>
    ::LLMEngine::AnalysisResult runAnalysis(Context& ctx,
                             std::string_view prompt,
                             const nlohmann::json& input,
                             std::string_view analysis_type,
                             std::string_view mode,
                             bool prepend_terse_instruction,
                             CleanupFunc&& cleanupResponseFiles,
                             EnsureSecureFunc&& ensureSecureTmpDir,
                             GetUniqueTmpDirFunc&& getUniqueTmpDir) {
        cleanupResponseFiles();

        // Build prompt using strategy pattern
        const std::string full_prompt = buildPrompt(prompt, prepend_terse_instruction);

        const bool write_debug_files = ctx.debug && (std::getenv("LLMENGINE_DISABLE_DEBUG_FILES") == nullptr);
        const std::string request_tmp_dir = getUniqueTmpDir();
        
        // Create debug artifact manager if needed
        std::unique_ptr<::LLMEngine::DebugArtifactManager> debug_mgr;
        if (write_debug_files) {
            ensureSecureTmpDir();
            debug_mgr = ArtifactSink::create(request_tmp_dir, ctx.tmp_dir, 
                                            ctx.log_retention_hours,
                                            ctx.logger ? ctx.logger.get() : nullptr);
        }

        // Merge parameters and execute request
        const nlohmann::json final_params = ::LLMEngine::ParameterMerger::merge(
            ctx.model_params, input, mode);
        const auto api_response = RequestExecutor::execute(
            ctx.api_client, full_prompt, input, final_params);

        // Write API response artifact
        ArtifactSink::writeApiResponse(debug_mgr.get(), api_response, !api_response.success);

        // Handle API response
        if (!api_response.success) {
            if (ctx.logger) {
                ctx.logger->log(::LLMEngine::LogLevel::Error, 
                               std::string("API error: ") + api_response.error_message);
                if (write_debug_files && debug_mgr) {
                    ctx.logger->log(::LLMEngine::LogLevel::Info, 
                                   std::string("Error response saved to ") + request_tmp_dir + "/api_response_error.json");
                }
            }
            return ::LLMEngine::AnalysisResult{false, "", "", api_response.error_message, api_response.status_code};
        }

        const std::string& full_response = api_response.content;

        // Write full response artifact
        ArtifactSink::writeFullResponse(debug_mgr.get(), full_response);

        // Parse and write analysis artifacts
        const auto [think_section, remaining_section] = ::LLMEngine::ResponseParser::parseResponse(full_response);
        ArtifactSink::writeAnalysisArtifacts(debug_mgr.get(), analysis_type, think_section, remaining_section);

        return ::LLMEngine::AnalysisResult{true, think_section, remaining_section, "", HTTP_STATUS_OK};
    }
}

// (constants are defined within LLMEngineSystem above)

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
            provider_type_, "", model_, ollama_url_);
    } else {
        api_client_ = ::LLMEngineAPI::APIClientFactory::createClient(
            provider_type_, api_key_, model_);
    }
    
    if (!api_client_) {
        throw std::runtime_error("Failed to create API client");
    }
}

// Redaction and debug file I/O moved to DebugArtifacts

void LLMEngine::LLMEngine::ensureSecureTmpDir() const {
    std::error_code ec;
    // Security: Check for symlink before creating directories
    // If the path exists and is a symlink, reject it to prevent symlink traversal attacks
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

void LLMEngine::LLMEngine::cleanupResponseFiles() const {
    // Legacy filename cleanup removed - we now use per-request directories
    // Old request directories are cleaned up by DebugArtifacts::cleanupOld()
    // This function is kept as a no-op for now to maintain the interface
}


::LLMEngine::AnalysisResult LLMEngine::LLMEngine::analyze(std::string_view prompt, 
                                  const nlohmann::json& input, 
                                  std::string_view analysis_type, 
                                  std::string_view mode,
                                  bool prepend_terse_instruction) const {
    // Create unique temp directory for this request to avoid conflicts in concurrent scenarios
    // Use efficient string building instead of ostringstream for better performance
    const auto now = std::chrono::system_clock::now();
    const auto time_since_epoch = now.time_since_epoch();
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(time_since_epoch).count();
    
    // Hash thread ID to hex string for efficient formatting (avoids locale-sensitive formatting)
    const auto thread_id = std::this_thread::get_id();
    std::hash<std::thread::id> hasher;
    const auto thread_hash = static_cast<uint64_t>(hasher(thread_id));
    
    // Pre-allocate string with estimated size
    std::string request_tmp_dir;
    constexpr size_t kRequestPathSuffixSize = 32; // Reserve space for "/req_" + milliseconds + "_" + hex hash
    request_tmp_dir.reserve(tmp_dir_.size() + kRequestPathSuffixSize);
    request_tmp_dir = tmp_dir_;
    request_tmp_dir += "/req_";
    request_tmp_dir += std::to_string(milliseconds);
    request_tmp_dir += "_";
    
    // Convert thread hash to hex string
    constexpr const char* hex_digits = "0123456789abcdef";
    constexpr int kHexBitsPerDigit = 4;
    constexpr int kHexStartBit = 60; // Start from bit 60 (64 - 4) for a 64-bit hash
    constexpr uint64_t kHexDigitMask = 0xF; // Mask to extract 4 bits (one hex digit)
    for (int i = kHexStartBit; i >= 0; i -= kHexBitsPerDigit) {
        request_tmp_dir += hex_digits[(thread_hash >> i) & kHexDigitMask];
    }
    
    LLMEngineSystem::Context ctx{
        model_params_, 
        log_retention_hours_, 
        debug_, 
        tmp_dir_,  // Base tmp dir for cleanup
        api_client_.get(),  // Pass raw pointer since we only need const access
        provider_type_, 
        logger_, 
        model_
    };
    return LLMEngineSystem::runAnalysis(ctx, prompt, input, analysis_type, mode, prepend_terse_instruction,
                      [this]{ this->cleanupResponseFiles(); },
                      [this]{ this->ensureSecureTmpDir(); },
                      [request_tmp_dir = std::move(request_tmp_dir)]{ return request_tmp_dir; });
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
    // Only accept directories within the default root to avoid accidental deletion elsewhere
    try {
        const std::filesystem::path default_root = std::filesystem::path(::LLMEngine::DefaultTempDirProvider().getTempDir()).lexically_normal();
        const std::filesystem::path requested    = std::filesystem::path(tmp_dir).lexically_normal();
        // Check prefix match: requested path must begin with default_root components
        auto it_def = default_root.begin();
        auto it_req = requested.begin();
        bool is_within = true;
        for (; it_def != default_root.end(); ++it_def, ++it_req) {
            if (it_req == requested.end() || *it_def != *it_req) { is_within = false; break; }
        }
        if (is_within) {
            tmp_dir_ = requested.string();
            return true;
        } else {
            if (logger_) {
                logger_->log(::LLMEngine::LogLevel::Warn, std::string("Rejected temp directory outside allowed root: ") + requested.string());
            }
            return false;
        }
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

