#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include <vector>
#include <memory>
#include "APIClient.hpp"

class LLMEngine {
public:
    // Legacy constructor for Ollama (backward compatibility)
    LLMEngine(const std::string& ollama_url, const std::string& model, const nlohmann::json& model_params = {}, int log_retention_hours = 24, bool debug = false);
    
    // New constructor for API-based providers
    LLMEngine(::LLMEngineAPI::ProviderType provider_type, const std::string& api_key, const std::string& model, const nlohmann::json& model_params = {}, int log_retention_hours = 24, bool debug = false);
    
    // Constructor using config file
    LLMEngine(const std::string& provider_name, const std::string& api_key = "", const std::string& model = "", const nlohmann::json& model_params = {}, int log_retention_hours = 24, bool debug = false);
    
    std::vector<std::string> analyze(const std::string& prompt, const nlohmann::json& input, const std::string& analysis_type, int max_tokens = 0, const std::string& mode = "chat") const;
    
    // Utility methods
    std::string getProviderName() const;
    ::LLMEngineAPI::ProviderType getProviderType() const;
    bool isOnlineProvider() const;
    
private:
    void cleanupResponseFiles() const;
    void initializeAPIClient();
    
    // Legacy Ollama fields (for backward compatibility)
    std::string ollama_url_;
    std::string model_;
    nlohmann::json model_params_;
    int log_retention_hours_;
    bool debug_;
    
    // New API client support
    std::unique_ptr<::LLMEngineAPI::APIClient> api_client_;
    ::LLMEngineAPI::ProviderType provider_type_;
    std::string api_key_;
    bool use_api_client_;
};