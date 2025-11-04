// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine.hpp"
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
#include <sstream>
#include "LLMOutputProcessor.hpp"
#include "Utils.hpp"
#include <filesystem>
#include <cstdlib>
#include "DebugArtifacts.hpp"

// Internal subwrapper namespace and Core orchestrator
namespace LLMEngineSystem {
    // Constants used by Core orchestration
    namespace {
        constexpr int HTTP_STATUS_OK = 200;
        constexpr int HTTP_STATUS_INTERNAL_SERVER_ERROR = 500;
        constexpr size_t REDACTED_REASONING_TAG_LENGTH = 20;  // "<think>" length
        constexpr size_t REDACTED_REASONING_CLOSE_TAG_LENGTH = 21;  // "</think>" length
        constexpr size_t MAX_FILENAME_LENGTH = 64;
    }
    struct Context {
        const nlohmann::json& model_params;
        int log_retention_hours;
        bool debug;
        const std::string& tmp_dir;
        const ::LLMEngineAPI::APIClient* api_client;
        ::LLMEngineAPI::ProviderType provider_type;
        const std::shared_ptr<Logger>& logger;
        const std::string& model;
    };

    // Free function instead of class with static method to avoid unnecessary allocation
    AnalysisResult runAnalysis(Context& ctx,
                             std::string_view prompt,
                             const nlohmann::json& input,
                             std::string_view analysis_type,
                             std::string_view mode,
                             bool prepend_terse_instruction,
                             const std::function<void()>& cleanupResponseFiles,
                             const std::function<void()>& ensureSecureTmpDir,
                             const std::function<std::string()>& getUniqueTmpDir) {
        cleanupResponseFiles();

        std::string full_prompt;
        if (prepend_terse_instruction) {
            std::string system_instruction =
                "Please respond directly to the previous message, engaging with its content. "
                "Try to be brief and concise and complete your response in one or two sentences, "
                "mostly one sentence.\n";
            full_prompt = system_instruction + std::string(prompt);
        } else {
            full_prompt = std::string(prompt);
        }

        std::string full_response;
        const bool write_debug_files = ctx.debug && (std::getenv("LLMENGINE_DISABLE_DEBUG_FILES") == nullptr);
        
        // Get unique temp directory for this request to avoid conflicts in concurrent scenarios
        const std::string request_tmp_dir = getUniqueTmpDir();

        if (ctx.api_client) {
            // Lazy merge: only copy keys that are actually overridden to avoid unnecessary allocations
            // Start with a reference to avoid copying when no overrides are needed
            const nlohmann::json* base_params = &ctx.model_params;
            nlohmann::json api_params;
            bool needs_copy = false;
            
            if (input.contains("max_tokens") && input["max_tokens"].is_number_integer()) {
                int max_tokens = input["max_tokens"].get<int>();
                if (max_tokens > 0) {
                    if (!needs_copy) {
                        api_params = *base_params;  // Copy only when first override is needed
                        needs_copy = true;
                    }
                    api_params["max_tokens"] = max_tokens;
                }
            }
            if (!std::string(mode).empty()) {
                if (!needs_copy) {
                    api_params = *base_params;  // Copy only when first override is needed
                    needs_copy = true;
                }
                api_params["mode"] = std::string(mode);
            }
            
            // Use reference if no overrides, otherwise use the modified copy
            const nlohmann::json& final_params = needs_copy ? api_params : *base_params;

            auto api_response = ctx.api_client->sendRequest(full_prompt, input, final_params);

            if (write_debug_files) {
                ensureSecureTmpDir();
                // Create unique directory for this request
                std::error_code ec_dir;
                std::filesystem::create_directories(request_tmp_dir, ec_dir);
                DebugArtifacts::writeJson(request_tmp_dir + "/api_response.json", api_response.raw_response, /*redactSecrets*/true);
                if (ctx.logger) ctx.logger->log(LogLevel::Debug, std::string("API response saved to ") + (request_tmp_dir + "/api_response.json"));
            }

            if (api_response.success) {
                full_response = api_response.content;
            } else {
                ensureSecureTmpDir();
                // Create unique directory for this request
                std::error_code ec_dir;
                std::filesystem::create_directories(request_tmp_dir, ec_dir);
                DebugArtifacts::writeJson(request_tmp_dir + "/api_response_error.json", api_response.raw_response, /*redactSecrets*/true);
                if (ctx.logger) ctx.logger->log(LogLevel::Error, std::string("API error: ") + api_response.error_message);
                if (ctx.logger) ctx.logger->log(LogLevel::Info, std::string("Error response saved to ") + (request_tmp_dir + "/api_response_error.json"));
                return AnalysisResult{false, "", "", api_response.error_message, api_response.status_code};
            }
        } else {
            return AnalysisResult{false, "", "", "API client not initialized", HTTP_STATUS_INTERNAL_SERVER_ERROR};
        }

        if (write_debug_files) {
            ensureSecureTmpDir();
            // Create unique directory for this request
            std::error_code ec_dir;
            std::filesystem::create_directories(request_tmp_dir, ec_dir);
            DebugArtifacts::writeText(request_tmp_dir + "/response_full.txt", full_response, /*redactSecrets*/true);
            if (ctx.logger) ctx.logger->log(LogLevel::Debug, std::string("Full response saved to ") + (request_tmp_dir + "/response_full.txt"));
            DebugArtifacts::cleanupOld(ctx.tmp_dir, ctx.log_retention_hours);
        }

        auto trim_copy = [](const std::string& s) -> std::string {
            const char* ws = " \t\n\r";
            std::string::size_type start = s.find_first_not_of(ws);
            if (start == std::string::npos) return {};
            std::string::size_type end = s.find_last_not_of(ws);
            return s.substr(start, end - start + 1);
        };

        std::string think_section;
        std::string remaining_section = full_response;
        std::string::size_type open = full_response.find("<redacted_reasoning>");
        std::string::size_type close = full_response.find("</redacted_reasoning>");
        if (open != std::string::npos && close != std::string::npos && close > open) {
            think_section = full_response.substr(open + REDACTED_REASONING_TAG_LENGTH, close - (open + REDACTED_REASONING_TAG_LENGTH));
            std::string before = full_response.substr(0, open);
            std::string after  = full_response.substr(close + REDACTED_REASONING_CLOSE_TAG_LENGTH);
            remaining_section = before + after;
        }
        think_section     = trim_copy(think_section);
        remaining_section = trim_copy(remaining_section);

        auto sanitize_name = [](std::string name) {
            if (name.empty()) name = "analysis";
            for (char& ch : name) {
                if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '-' || ch == '_' || ch == '.')) {
                    ch = '_';
                }
            }
            if (name.size() > MAX_FILENAME_LENGTH) name.resize(MAX_FILENAME_LENGTH);
            return name;
        };
        const std::string safe_analysis_name = sanitize_name(std::string(analysis_type));
        if (write_debug_files) {
            ensureSecureTmpDir();
            // Create unique directory for this request (if not already created)
            std::error_code ec_dir;
            std::filesystem::create_directories(request_tmp_dir, ec_dir);
            DebugArtifacts::writeText(request_tmp_dir + "/" + safe_analysis_name + ".think.txt", think_section, /*redactSecrets*/true);
            if (ctx.logger) ctx.logger->log(LogLevel::Debug, "Wrote think section");
            DebugArtifacts::writeText(request_tmp_dir + "/" + safe_analysis_name + ".txt", remaining_section, /*redactSecrets*/true);
            if (ctx.logger) ctx.logger->log(LogLevel::Debug, "Wrote remaining section");
        }

        return AnalysisResult{true, think_section, remaining_section, "", HTTP_STATUS_OK};
    }
}

// (constants are defined within LLMEngineSystem above)

// Dependency injection constructor
LLMEngine::LLMEngine(std::unique_ptr<::LLMEngineAPI::APIClient> client,
                     const nlohmann::json& model_params,
                     int log_retention_hours,
                     bool debug)
    : model_params_(model_params),
      log_retention_hours_(log_retention_hours),
      debug_(debug),
      tmp_dir_(Utils::TMP_DIR),
      api_client_(std::move(client)) {
    logger_ = std::make_shared<DefaultLogger>();
    if (!api_client_) {
        throw std::runtime_error("API client must not be null");
    }
    provider_type_ = api_client_->getProviderType();
}

// New constructor for API-based providers
LLMEngine::LLMEngine(::LLMEngineAPI::ProviderType provider_type,
                     std::string_view api_key,
                     std::string_view model,
                     const nlohmann::json& model_params,
                     int log_retention_hours,
                     bool debug)
    : model_(std::string(model)),
      model_params_(model_params),
      log_retention_hours_(log_retention_hours),
      debug_(debug),
      tmp_dir_(Utils::TMP_DIR),
      provider_type_(provider_type),
      api_key_(std::string(api_key)) {
    logger_ = std::make_shared<DefaultLogger>();
    initializeAPIClient();
}

// Constructor using config file
LLMEngine::LLMEngine(std::string_view provider_name,
                     std::string_view api_key,
                     std::string_view model,
                     const nlohmann::json& model_params,
                     int log_retention_hours,
                     bool debug)
    : model_params_(model_params),
      log_retention_hours_(log_retention_hours),
      debug_(debug),
      tmp_dir_(Utils::TMP_DIR),
      api_key_(std::string(api_key)) {
    logger_ = std::make_shared<DefaultLogger>();
    
    // Load config
    auto& config_mgr = ::LLMEngineAPI::APIConfigManager::getInstance();
    if (!config_mgr.loadConfig()) {
        logger_->log(LogLevel::Warn, "Could not load API config, using defaults");
    }
    
    // Determine provider name (use default if empty)
    std::string resolved_provider(provider_name);
    if (resolved_provider.empty()) {
        resolved_provider = config_mgr.getDefaultProvider();
        if (resolved_provider.empty()) {
            resolved_provider = "ollama";
        }
    }

    // Get provider configuration
    auto provider_config = config_mgr.getProviderConfig(resolved_provider);
    if (provider_config.empty()) {
        logger_->log(LogLevel::Error, std::string("Provider ") + resolved_provider + " not found in config");
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
    const char* env_api_key = std::getenv(env_var_name.c_str());
    if (env_api_key && strlen(env_api_key) > 0) {
        api_key_ = env_api_key;
    } else if (!std::string(api_key).empty()) {
        // Use provided API key if environment variable is not set
        api_key_ = std::string(api_key);
    } else {
        // Fall back to config file (last resort - not recommended for production)
        api_key_ = provider_config.value("api_key", "");
        if (!api_key_.empty()) {
            logger_->log(LogLevel::Warn, std::string("Using API key from config file. For production use, ")
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

void LLMEngine::initializeAPIClient() {
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

void LLMEngine::ensureSecureTmpDir() const {
    std::error_code ec;
    // Security: Check for symlink before creating directories
    // If the path exists and is a symlink, reject it to prevent symlink traversal attacks
    if (std::filesystem::exists(tmp_dir_, ec)) {
        std::error_code ec_symlink;
        if (std::filesystem::is_symlink(tmp_dir_, ec_symlink)) {
            if (logger_) {
                logger_->log(LogLevel::Error, std::string("Temporary directory is a symlink: ") + tmp_dir_);
            }
            throw std::runtime_error("Temporary directory cannot be a symlink for security reasons: " + tmp_dir_);
        }
    }
    
    std::filesystem::create_directories(tmp_dir_, ec);
    if (ec) {
        if (logger_) {
            logger_->log(LogLevel::Error, std::string("Failed to create temporary directory: ") + tmp_dir_ + ": " + ec.message());
        }
        throw std::runtime_error("Failed to create temporary directory: " + tmp_dir_);
    }
    
    if (std::filesystem::exists(tmp_dir_)) {
        std::error_code ec_perm;
        std::filesystem::permissions(tmp_dir_, 
            std::filesystem::perms::owner_read | std::filesystem::perms::owner_write | std::filesystem::perms::owner_exec,
            std::filesystem::perm_options::replace, ec_perm);
        if (ec_perm && logger_) {
            logger_->log(LogLevel::Warn, std::string("Failed to set permissions on ") + tmp_dir_ + ": " + ec_perm.message());
        }
    }
}

void LLMEngine::cleanupResponseFiles() const {
    std::vector<std::string> basenames_to_clean = {
        "ollama_response.json",
        "ollama_response_full.txt",
        "ollama_response_error.json",
        "api_response.json",
        "api_response_error.json",
        "response_full.txt"
    };

    ensureSecureTmpDir();

    int cleaned_count = 0;
    const std::filesystem::path base_dir = std::filesystem::path(tmp_dir_);
    for (const auto& basename : basenames_to_clean) {
        const std::filesystem::path candidate = base_dir / basename;
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec) && std::filesystem::is_regular_file(candidate, ec)) {
            if (std::filesystem::remove(candidate, ec)) {
                cleaned_count++;
                if (debug_ && logger_) {
                    logger_->log(LogLevel::Debug, std::string("Cleaned up existing file: ") + candidate.string());
                }
            } else if (ec && logger_) {
                logger_->log(LogLevel::Warn, std::string("Failed to remove ") + candidate.string() + ": " + ec.message());
            }
        }
    }

    if (cleaned_count > 0 && debug_ && logger_) {
        logger_->log(LogLevel::Debug, std::string("Cleaned up ") + std::to_string(cleaned_count) + " existing response file(s) in " + tmp_dir_);
    }
}


AnalysisResult LLMEngine::analyze(std::string_view prompt, 
                                  const nlohmann::json& input, 
                                  std::string_view analysis_type, 
                                  std::string_view mode,
                                  bool prepend_terse_instruction) const {
    // Create unique temp directory for this request to avoid conflicts in concurrent scenarios
    const auto now = std::chrono::system_clock::now();
    const auto time_since_epoch = now.time_since_epoch();
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(time_since_epoch).count();
    const auto thread_id = std::this_thread::get_id();
    std::ostringstream oss;
    oss << tmp_dir_ << "/req_" << milliseconds << "_" << thread_id;
    const std::string request_tmp_dir = oss.str();
    
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
                      [request_tmp_dir]{ return request_tmp_dir; });
}

std::string LLMEngine::getProviderName() const {
    if (api_client_) {
        return api_client_->getProviderName();
    }
    return "Ollama (Legacy)";
}

std::string LLMEngine::getModelName() const {
    return model_;
}

::LLMEngineAPI::ProviderType LLMEngine::getProviderType() const {
    return provider_type_;
}

bool LLMEngine::isOnlineProvider() const {
    return provider_type_ != ::LLMEngineAPI::ProviderType::OLLAMA;
}

void LLMEngine::setLogger(std::shared_ptr<Logger> logger) {
    if (logger) {
        logger_ = std::move(logger);
    }
}

void LLMEngine::setTempDirectory(const std::string& tmp_dir) {
    // Only accept directories within the default root to avoid accidental deletion elsewhere
    try {
        const std::filesystem::path default_root = std::filesystem::path(Utils::TMP_DIR).lexically_normal();
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
        } else if (logger_) {
            logger_->log(LogLevel::Warn, std::string("Rejected temp directory outside allowed root: ") + requested.string());
        }
    } catch (...) {
        if (logger_) {
            logger_->log(LogLevel::Error, std::string("Failed to set temp directory due to path error: ") + tmp_dir);
        }
    }
}

std::string LLMEngine::getTempDirectory() const {
    return tmp_dir_;
}

