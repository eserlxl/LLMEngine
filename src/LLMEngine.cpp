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
#include "LLMOutputProcessor.hpp"
#include "Utils.hpp"
#include <filesystem>
#include <cstdlib>

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
    
    // Load config
    auto& config_mgr = ::LLMEngineAPI::APIConfigManager::getInstance();
    if (!config_mgr.loadConfig()) {
        std::cerr << "[WARNING] Could not load API config, using defaults" << std::endl;
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
        std::cerr << "[ERROR] Provider " << resolved_provider << " not found in config" << std::endl;
        throw std::runtime_error("Invalid provider name");
    }
    
    // Set model
    if (std::string(model).empty()) {
        model_ = provider_config.value("default_model", "");
    } else {
        model_ = std::string(model);
    }
    
    // Set provider type
    provider_type_ = ::LLMEngineAPI::APIClientFactory::stringToProviderType(resolved_provider);
    
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

    int cleaned_count = 0;
    for (const auto& suffix : files_to_clean) {
        const std::string path = Utils::TMP_DIR + suffix;
        std::error_code ec;
        if (std::filesystem::exists(path, ec) && std::filesystem::is_regular_file(path, ec)) {
            if (std::filesystem::remove(path, ec)) {
                cleaned_count++;
                if (debug_) {
                    std::cout << "[DEBUG] Cleaned up existing file: " << path << std::endl;
                }
            } else if (ec) {
                std::cerr << "[WARNING] Failed to remove " << path << ": " << ec.message() << std::endl;
            }
        }
    }

    if (cleaned_count > 0 && debug_) {
        std::cout << "[DEBUG] Cleaned up " << cleaned_count << " existing response file(s) in " << Utils::TMP_DIR << std::endl;
    }
}

std::vector<std::string> LLMEngine::analyze(std::string_view prompt, 
                                           const nlohmann::json& input, 
                                           std::string_view analysis_type, 
                                           std::string_view mode) const {
    // Clean up existing response files
    cleanupResponseFiles();
    
    // Prepend short-answer instruction
    std::string system_instruction = 
        "Please respond directly to the previous message, engaging with its content. "
        "Try to be brief and concise and complete your response in one or two sentences, "
        "mostly one sentence.\n";
    std::string full_prompt = system_instruction + std::string(prompt);
    
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
            std::error_code ec_dir;
            std::filesystem::create_directories(Utils::TMP_DIR, ec_dir);
            std::ofstream resp_file(Utils::TMP_DIR + "/api_response.json");
            resp_file << api_response.raw_response.dump(2);
            resp_file.close();
            std::cout << "[DEBUG] API response saved to " << (Utils::TMP_DIR + "/api_response.json") << std::endl;
        }
        
        if (api_response.success) {
            full_response = api_response.content;
        } else {
            std::error_code ec_dir2;
            std::filesystem::create_directories(Utils::TMP_DIR, ec_dir2);
            std::ofstream err_file(Utils::TMP_DIR + "/api_response_error.json");
            err_file << api_response.raw_response.dump(2);
            err_file.close();
            std::cerr << "[ERROR] API error: " << api_response.error_message << std::endl;
            std::cerr << "[INFO] Error response saved to " << (Utils::TMP_DIR + "/api_response_error.json") << std::endl;
            return {"[LLMEngine] Error: " + api_response.error_message, ""};
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
            std::error_code ec_dir3;
            std::filesystem::create_directories(Utils::TMP_DIR, ec_dir3);
            std::ofstream resp_file(Utils::TMP_DIR + "/ollama_response.json");
            resp_file << response.text;
            resp_file.close();
            std::cout << "[DEBUG] Ollama response saved to " << (Utils::TMP_DIR + "/ollama_response.json") << std::endl;
        }
        
        if (response.status_code != 200) {
            std::error_code ec_dir4;
            std::filesystem::create_directories(Utils::TMP_DIR, ec_dir4);
            std::ofstream err_file(Utils::TMP_DIR + "/ollama_response_error.json");
            err_file << response.text;
            err_file.close();
            std::cerr << "[ERROR] Ollama error response saved to " << (Utils::TMP_DIR + "/ollama_response_error.json") << std::endl;
            return {"[LLMEngine] Error: Failed to contact Ollama server.", ""};
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
                std::cerr << "[ERROR] Failed to parse NDJSON line: " << e.what() << std::endl;
            }
        }
    }
    
    if (write_debug_files) {
        std::error_code ec_dir5;
        std::filesystem::create_directories(Utils::TMP_DIR, ec_dir5);
        std::ofstream full_file(Utils::TMP_DIR + "/response_full.txt");
        full_file << full_response;
        full_file.close();
        std::cout << "[DEBUG] Full response saved to " << (Utils::TMP_DIR + "/response_full.txt") << std::endl;
    }
    
    // Extract THINK section
    auto trim_copy = [](const std::string& s) -> std::string {
        const char* ws = " \t\n\r";
        std::string::size_type start = s.find_first_not_of(ws);
        if (start == std::string::npos) return std::string();
        std::string::size_type end = s.find_last_not_of(ws);
        return s.substr(start, end - start + 1);
    };
    
    std::string think_section;
    std::string remaining_section = full_response;
    
    std::string::size_type open = full_response.find("<redacted_reasoning>");
    std::string::size_type close = full_response.find("</redacted_reasoning>");
    
    if (open != std::string::npos && close != std::string::npos && close > open) {
        think_section = full_response.substr(open + 20, close - (open + 20));
        std::string before = full_response.substr(0, open);
        std::string after  = full_response.substr(close + 21);
        remaining_section = before + after;
    }
    
    think_section     = trim_copy(think_section);
    remaining_section = trim_copy(remaining_section);
    
    // Ensure temporary directory exists
    std::error_code ec;
    std::filesystem::create_directories(Utils::TMP_DIR, ec);
    
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
        if (name.size() > 64) name.resize(64);
        return name;
    };
    const std::string safe_analysis_name = sanitize_name(std::string(analysis_type));
    try {
        std::ofstream tfile(Utils::TMP_DIR + "/" + safe_analysis_name + ".think.txt", std::ios::trunc);
        if (!tfile) throw std::runtime_error("open failed");
        tfile << think_section;
        if (debug_) std::cout << "[DEBUG] Wrote think section\n";
    } catch (...) {
        std::cerr << "[ERROR] Could not write think file\n";
    }
    
    try {
        std::ofstream rfile(Utils::TMP_DIR + "/" + safe_analysis_name + ".txt", std::ios::trunc);
        if (!rfile) throw std::runtime_error("open failed");
        rfile << remaining_section;
        if (debug_) std::cout << "[DEBUG] Wrote remaining section\n";
    } catch (...) {
        std::cerr << "[ERROR] Could not write remaining file\n";
    }
    
    return {think_section, remaining_section};
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

