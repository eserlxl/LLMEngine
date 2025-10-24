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

// Legacy constructor for Ollama (backward compatibility)
LLMEngine::LLMEngine(const std::string& ollama_url, 
                     const std::string& model, 
                     const nlohmann::json& model_params, 
                     int log_retention_hours, 
                     bool debug)
    : ollama_url_(ollama_url), 
      model_(model), 
      model_params_(model_params), 
      log_retention_hours_(log_retention_hours), 
      debug_(debug),
      provider_type_(::LLMEngineAPI::ProviderType::OLLAMA),
      use_api_client_(false) {
    // Legacy mode - use direct HTTP calls to Ollama
}

// New constructor for API-based providers
LLMEngine::LLMEngine(::LLMEngineAPI::ProviderType provider_type,
                     const std::string& api_key,
                     const std::string& model,
                     const nlohmann::json& model_params,
                     int log_retention_hours,
                     bool debug)
    : model_(model),
      model_params_(model_params),
      log_retention_hours_(log_retention_hours),
      debug_(debug),
      provider_type_(provider_type),
      api_key_(api_key),
      use_api_client_(true) {
    initializeAPIClient();
}

// Constructor using config file
LLMEngine::LLMEngine(const std::string& provider_name,
                     const std::string& api_key,
                     const std::string& model,
                     const nlohmann::json& model_params,
                     int log_retention_hours,
                     bool debug)
    : model_params_(model_params),
      log_retention_hours_(log_retention_hours),
      debug_(debug),
      api_key_(api_key),
      use_api_client_(true) {
    
    // Load config
    auto& config_mgr = ::LLMEngineAPI::APIConfigManager::getInstance();
    if (!config_mgr.loadConfig()) {
        std::cerr << "[WARNING] Could not load API config, using defaults" << std::endl;
    }
    
    // Get provider configuration
    auto provider_config = config_mgr.getProviderConfig(provider_name);
    if (provider_config.empty()) {
        std::cerr << "[ERROR] Provider " << provider_name << " not found in config" << std::endl;
        throw std::runtime_error("Invalid provider name");
    }
    
    // Set model
    if (model.empty()) {
        model_ = provider_config.value("default_model", "");
    } else {
        model_ = model;
    }
    
    // Set provider type
    provider_type_ = ::LLMEngineAPI::APIClientFactory::stringToProviderType(provider_name);
    
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
        "ollama_response.json",
        "ollama_response_full.txt",
        "ollama_response_error.json",
        "api_response.json",
        "api_response_error.json"
    };
    
    int cleaned_count = 0;
    for (const auto& filename : files_to_clean) {
        if (std::filesystem::exists(filename)) {
            try {
                std::filesystem::remove(filename);
                cleaned_count++;
                if (debug_) {
                    std::cout << "[DEBUG] Cleaned up existing file: " << filename << std::endl;
                }
            } catch (const std::filesystem::filesystem_error& e) {
                std::cerr << "[WARNING] Failed to remove " << filename << ": " << e.what() << std::endl;
            }
        }
    }
    
    if (cleaned_count > 0 && debug_) {
        std::cout << "[DEBUG] Cleaned up " << cleaned_count << " existing response file(s)" << std::endl;
    }
}

std::vector<std::string> LLMEngine::analyze(const std::string& prompt, 
                                           const nlohmann::json& input, 
                                           const std::string& analysis_type, 
                                           int max_tokens) const {
    // Clean up existing response files
    cleanupResponseFiles();
    
    // Prepend short-answer instruction
    std::string system_instruction = 
        "Please respond directly to the previous message, engaging with its content. "
        "Try to be brief and concise and complete your response in one or two sentences, "
        "mostly one sentence.\n";
    std::string full_prompt = system_instruction + prompt;
    
    std::string full_response;
    
    if (use_api_client_ && api_client_) {
        // Use API client
        nlohmann::json api_params = model_params_;
        if (max_tokens > 0) {
            api_params["max_tokens"] = max_tokens;
        }
        
        auto api_response = api_client_->sendRequest(full_prompt, input, api_params);
        
        if (debug_) {
            std::ofstream resp_file("api_response.json");
            resp_file << api_response.raw_response.dump(2);
            resp_file.close();
            std::cout << "[DEBUG] API response saved to api_response.json" << std::endl;
        }
        
        if (api_response.success) {
            full_response = api_response.content;
        } else {
            std::ofstream err_file("api_response_error.json");
            err_file << api_response.raw_response.dump(2);
            err_file.close();
            std::cerr << "[ERROR] API error: " << api_response.error_message << std::endl;
            std::cerr << "[INFO] Error response saved to api_response_error.json" << std::endl;
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
        if (max_tokens > 0) {
            payload["max_tokens"] = max_tokens;
        }
        
        auto response = cpr::Post(cpr::Url{ollama_url_ + "/api/generate"},
                                  cpr::Header{{"Content-Type", "application/json"}},
                                  cpr::Body{payload.dump()});
        
        if (debug_) {
            std::ofstream resp_file("ollama_response.json");
            resp_file << response.text;
            resp_file.close();
            std::cout << "[DEBUG] Ollama response saved to ollama_response.json" << std::endl;
        }
        
        if (response.status_code != 200) {
            std::ofstream err_file("ollama_response_error.json");
            err_file << response.text;
            err_file.close();
            std::cerr << "[ERROR] Ollama error response saved to ollama_response_error.json" << std::endl;
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
    
    if (debug_) {
        std::ofstream full_file("response_full.txt");
        full_file << full_response;
        full_file.close();
        std::cout << "[DEBUG] Full response saved to response_full.txt" << std::endl;
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
    try {
        std::ofstream tfile(Utils::TMP_DIR + "/" + analysis_type + ".think.txt", std::ios::trunc);
        tfile << think_section;
        if (debug_) std::cout << "[DEBUG] Wrote think section\n";
    } catch (...) {
        std::cerr << "[ERROR] Could not write think file\n";
    }
    
    try {
        std::ofstream rfile(Utils::TMP_DIR + "/" + analysis_type + ".txt", std::ios::trunc);
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

::LLMEngineAPI::ProviderType LLMEngine::getProviderType() const {
    return provider_type_;
}

bool LLMEngine::isOnlineProvider() const {
    return use_api_client_ && provider_type_ != ::LLMEngineAPI::ProviderType::OLLAMA;
}

