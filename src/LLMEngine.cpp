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
#include "LLMOutputProcessor.hpp"
#include "Utils.hpp"
#include <filesystem>
#include <cstdlib>
#include "DebugArtifacts.hpp"

// Constants
namespace {
    constexpr int HTTP_STATUS_OK = 200;
    constexpr size_t REDACTED_REASONING_TAG_LENGTH = 20;  // "<think>" length
    constexpr size_t REDACTED_REASONING_CLOSE_TAG_LENGTH = 21;  // "</think>" length
    constexpr size_t MAX_FILENAME_LENGTH = 64;
}

// Legacy constructor for Ollama (backward compatibility)
LLMEngine::LLMEngine(std::string_view ollama_url, 
                     std::string_view model, 
                     const nlohmann::json& model_params, 
                     int log_retention_hours, 
                     bool debug)
    : ollama_url_(std::string(ollama_url)), 
      model_(std::string(model)), 
      model_params_(model_params), 
      log_retention_hours_(log_retention_hours), 
      debug_(debug),
      provider_type_(::LLMEngineAPI::ProviderType::OLLAMA),
      use_api_client_(false) {
    logger_ = std::make_shared<DefaultLogger>();
    // Legacy mode - use direct HTTP calls to Ollama
}

// Dependency injection constructor
LLMEngine::LLMEngine(std::unique_ptr<::LLMEngineAPI::APIClient> client,
                     const nlohmann::json& model_params,
                     int log_retention_hours,
                     bool debug)
    : model_params_(model_params),
      log_retention_hours_(log_retention_hours),
      debug_(debug),
      api_client_(std::move(client)),
      use_api_client_(true) {
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
      provider_type_(provider_type),
      api_key_(std::string(api_key)),
      use_api_client_(true) {
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
      api_key_(std::string(api_key)),
      use_api_client_(true) {
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

void LLMEngine::cleanupResponseFiles() const {
    std::vector<std::string> files_to_clean = {
        "/ollama_response.json",
        "/ollama_response_full.txt",
        "/ollama_response_error.json",
        "/api_response.json",
        "/api_response_error.json",
        "/response_full.txt"
    };

    std::error_code ec_dir;
    std::filesystem::create_directories(Utils::TMP_DIR, ec_dir);
    // Set directory permissions to 0700 (owner-only read/write/execute) for security
    if (!ec_dir && std::filesystem::exists(Utils::TMP_DIR)) {
        std::error_code ec_perm;
        std::filesystem::permissions(Utils::TMP_DIR, 
            std::filesystem::perms::owner_read | std::filesystem::perms::owner_write | std::filesystem::perms::owner_exec,
            std::filesystem::perm_options::replace, ec_perm);
        if (ec_perm && logger_) {
            logger_->log(LogLevel::Warn, std::string("Failed to set permissions on ") + Utils::TMP_DIR + ": " + ec_perm.message());
        }
    }

    int cleaned_count = 0;
    for (const auto& suffix : files_to_clean) {
        const std::string path = Utils::TMP_DIR + suffix;
        std::error_code ec;
        if (std::filesystem::exists(path, ec) && std::filesystem::is_regular_file(path, ec)) {
            if (std::filesystem::remove(path, ec)) {
                cleaned_count++;
                if (debug_ && logger_) {
                    logger_->log(LogLevel::Debug, std::string("Cleaned up existing file: ") + path);
                }
            } else if (ec && logger_) {
                logger_->log(LogLevel::Warn, std::string("Failed to remove ") + path + ": " + ec.message());
            }
        }
    }

    if (cleaned_count > 0 && debug_ && logger_) {
        logger_->log(LogLevel::Debug, std::string("Cleaned up ") + std::to_string(cleaned_count) + " existing response file(s) in " + Utils::TMP_DIR);
    }
}


AnalysisResult LLMEngine::analyze(std::string_view prompt, 
                                  const nlohmann::json& input, 
                                  std::string_view analysis_type, 
                                  std::string_view mode,
                                  bool prepend_terse_instruction) const {
    // Clean up existing response files
    cleanupResponseFiles();
    
    // Cache temporary directory setup to avoid repeated work
    static std::once_flag tmp_dir_initialized;
    static bool tmp_dir_ready = false;
    std::call_once(tmp_dir_initialized, []() {
        std::error_code ec;
        std::filesystem::create_directories(Utils::TMP_DIR, ec);
        if (!ec && std::filesystem::exists(Utils::TMP_DIR)) {
            std::error_code ec_perm;
            std::filesystem::permissions(Utils::TMP_DIR, 
                std::filesystem::perms::owner_read | std::filesystem::perms::owner_write | std::filesystem::perms::owner_exec,
                std::filesystem::perm_options::replace, ec_perm);
            // Best-effort: silently ignore permission errors (may fail in restricted environments)
            tmp_dir_ready = true;
        }
    });
    
    // Helper to ensure TMP_DIR exists with secure permissions (0700) - only if not already initialized
    auto ensureSecureTmpDir = [this]() {
        if (!tmp_dir_ready) {
            std::error_code ec;
            std::filesystem::create_directories(Utils::TMP_DIR, ec);
            if (!ec && std::filesystem::exists(Utils::TMP_DIR)) {
                std::error_code ec_perm;
                std::filesystem::permissions(Utils::TMP_DIR, 
                    std::filesystem::perms::owner_read | std::filesystem::perms::owner_write | std::filesystem::perms::owner_exec,
                    std::filesystem::perm_options::replace, ec_perm);
                if (ec_perm && logger_) {
                    logger_->log(LogLevel::Warn, std::string("Failed to set permissions on ") + Utils::TMP_DIR + ": " + ec_perm.message());
                }
            }
        }
    };
    
    // Optionally prepend short-answer instruction
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
    
    const bool write_debug_files = debug_ && (std::getenv("LLMENGINE_DISABLE_DEBUG_FILES") == nullptr);

    if (use_api_client_ && api_client_) {
        // Use API client
        nlohmann::json api_params = model_params_;
        
        // Get max_tokens from input payload if provided
        if (input.contains("max_tokens") && input["max_tokens"].is_number_integer()) {
            int max_tokens = input["max_tokens"].get<int>();
            if (max_tokens > 0) {
                api_params["max_tokens"] = max_tokens;
            }
        }
        
    if (!std::string(mode).empty()) {
            api_params["mode"] = std::string(mode);
        }
        
        auto api_response = api_client_->sendRequest(full_prompt, input, api_params);
        
        if (write_debug_files) {
            ensureSecureTmpDir();
            
            DebugArtifacts::writeJson(Utils::TMP_DIR + "/api_response.json", api_response.raw_response, /*redactSecrets*/true);
            logger_->log(LogLevel::Debug, std::string("API response saved to ") + (Utils::TMP_DIR + "/api_response.json"));
        }
        
        if (api_response.success) {
            full_response = api_response.content;
        } else {
            ensureSecureTmpDir();
            
            DebugArtifacts::writeJson(Utils::TMP_DIR + "/api_response_error.json", api_response.raw_response, /*redactSecrets*/true);
            logger_->log(LogLevel::Error, std::string("API error: ") + api_response.error_message);
            logger_->log(LogLevel::Info, std::string("Error response saved to ") + (Utils::TMP_DIR + "/api_response_error.json"));
            return AnalysisResult{false, "", "", api_response.error_message, api_response.status_code};
        }
    } else {
        // Legacy Ollama mode
        nlohmann::json payload = {
            {"model", model_},
            {"prompt", full_prompt},
            {"input", input}
        };
        
        for (auto& [key, value] : model_params_.items()) {
            payload[key] = value;
        }
        
        // Get max_tokens from input payload if provided
        if (input.contains("max_tokens") && input["max_tokens"].is_number_integer()) {
            int max_tokens = input["max_tokens"].get<int>();
            if (max_tokens > 0) {
                payload["max_tokens"] = max_tokens;
            }
        }
        
        auto response = cpr::Post(cpr::Url{ollama_url_ + "/api/generate"},
                                  cpr::Header{{"Content-Type", "application/json"}},
                                  cpr::Body{payload.dump()});
        
        if (write_debug_files) {
            ensureSecureTmpDir();
            DebugArtifacts::writeText(Utils::TMP_DIR + "/ollama_response.json", response.text, /*redactSecrets*/true);
            logger_->log(LogLevel::Debug, std::string("Ollama response saved to ") + (Utils::TMP_DIR + "/ollama_response.json"));
        }
        
        if (response.status_code != HTTP_STATUS_OK) {
            ensureSecureTmpDir();
            DebugArtifacts::writeText(Utils::TMP_DIR + "/ollama_response_error.json", response.text, /*redactSecrets*/true);
            logger_->log(LogLevel::Error, std::string("Ollama error response saved to ") + (Utils::TMP_DIR + "/ollama_response_error.json"));
            return AnalysisResult{false, "", "", "Failed to contact Ollama server.", static_cast<int>(response.status_code)};
        }
        
        // Reconstruct full response from NDJSON
        std::istringstream resp_stream(response.text);
        std::string line;
        while (std::getline(resp_stream, line)) {
            try {
                auto line_json = nlohmann::json::parse(line);
                if (line_json.contains("response")) {
                    full_response += line_json["response"].get<std::string>();
                }
            } catch (const std::exception& e) {
                logger_->log(LogLevel::Error, std::string("Failed to parse NDJSON line: ") + e.what());
            }
        }
    }
    
    if (write_debug_files) {
        ensureSecureTmpDir();
        
        DebugArtifacts::writeText(Utils::TMP_DIR + "/response_full.txt", full_response, /*redactSecrets*/true);
        logger_->log(LogLevel::Debug, std::string("Full response saved to ") + (Utils::TMP_DIR + "/response_full.txt"));
        
        // Clean up old debug files based on retention policy
        DebugArtifacts::cleanupOld(Utils::TMP_DIR, log_retention_hours_);
    }
    
    // Extract THINK section
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
    
    // Ensure temporary directory exists with secure permissions
    ensureSecureTmpDir();
    
    // Write files
    auto sanitize_name = [](std::string name) {
        if (name.empty()) name = "analysis";
        // Replace path separators and whitespace with underscore; keep simple ascii
        for (char& ch : name) {
            if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '-' || ch == '_' || ch == '.')) {
                ch = '_';
            }
        }
        // Trim excessive length
        if (name.size() > MAX_FILENAME_LENGTH) name.resize(MAX_FILENAME_LENGTH);
        return name;
    };
    const std::string safe_analysis_name = sanitize_name(std::string(analysis_type));
    DebugArtifacts::writeText(Utils::TMP_DIR + "/" + safe_analysis_name + ".think.txt", think_section, /*redactSecrets*/true);
    if (debug_) logger_->log(LogLevel::Debug, "Wrote think section");
    
    DebugArtifacts::writeText(Utils::TMP_DIR + "/" + safe_analysis_name + ".txt", remaining_section, /*redactSecrets*/true);
    if (debug_) logger_->log(LogLevel::Debug, "Wrote remaining section");
    
    return AnalysisResult{true, think_section, remaining_section, "", HTTP_STATUS_OK};
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
    return use_api_client_ && provider_type_ != ::LLMEngineAPI::ProviderType::OLLAMA;
}

void LLMEngine::setLogger(std::shared_ptr<Logger> logger) {
    if (logger) {
        logger_ = std::move(logger);
    }
}

