#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include <memory>
#include <map>
#include <vector>

namespace LLMEngineAPI {

enum class ProviderType {
    QWEN,
    OPENAI,
    ANTHROPIC,
    OLLAMA
};

struct APIResponse {
    bool success;
    std::string content;
    std::string error_message;
    int status_code;
    nlohmann::json raw_response;
};

class APIClient {
public:
    virtual ~APIClient() = default;
    virtual APIResponse sendRequest(const std::string& prompt, 
                                   const nlohmann::json& input,
                                   const nlohmann::json& params) const = 0;
    virtual std::string getProviderName() const = 0;
    virtual ProviderType getProviderType() const = 0;
};

class QwenClient : public APIClient {
public:
    QwenClient(const std::string& api_key, const std::string& model = "qwen-flash");
    APIResponse sendRequest(const std::string& prompt, 
                           const nlohmann::json& input,
                           const nlohmann::json& params) const override;
    std::string getProviderName() const override { return "Qwen"; }
    ProviderType getProviderType() const override { return ProviderType::QWEN; }

private:
    std::string api_key_;
    std::string model_;
    std::string base_url_;
    nlohmann::json default_params_;
};

class OpenAIClient : public APIClient {
public:
    OpenAIClient(const std::string& api_key, const std::string& model = "gpt-3.5-turbo");
    APIResponse sendRequest(const std::string& prompt, 
                           const nlohmann::json& input,
                           const nlohmann::json& params) const override;
    std::string getProviderName() const override { return "OpenAI"; }
    ProviderType getProviderType() const override { return ProviderType::OPENAI; }

private:
    std::string api_key_;
    std::string model_;
    std::string base_url_;
    nlohmann::json default_params_;
};

class AnthropicClient : public APIClient {
public:
    AnthropicClient(const std::string& api_key, const std::string& model = "claude-3-sonnet");
    APIResponse sendRequest(const std::string& prompt, 
                           const nlohmann::json& input,
                           const nlohmann::json& params) const override;
    std::string getProviderName() const override { return "Anthropic"; }
    ProviderType getProviderType() const override { return ProviderType::ANTHROPIC; }

private:
    std::string api_key_;
    std::string model_;
    std::string base_url_;
    nlohmann::json default_params_;
};

class OllamaClient : public APIClient {
public:
    OllamaClient(const std::string& base_url = "http://localhost:11434", 
                 const std::string& model = "llama2");
    APIResponse sendRequest(const std::string& prompt, 
                           const nlohmann::json& input,
                           const nlohmann::json& params) const override;
    std::string getProviderName() const override { return "Ollama"; }
    ProviderType getProviderType() const override { return ProviderType::OLLAMA; }

private:
    std::string base_url_;
    std::string model_;
    nlohmann::json default_params_;
};

class APIClientFactory {
public:
    static std::unique_ptr<APIClient> createClient(ProviderType type, 
                                                   const std::string& api_key = "",
                                                   const std::string& model = "",
                                                   const std::string& base_url = "");
    
    static std::unique_ptr<APIClient> createClientFromConfig(const std::string& provider_name,
                                                             const nlohmann::json& config);
    
    static ProviderType stringToProviderType(const std::string& provider_name);
    static std::string providerTypeToString(ProviderType type);
};

class APIConfigManager {
public:
    static APIConfigManager& getInstance();
    
    bool loadConfig(const std::string& config_path = "");
    nlohmann::json getProviderConfig(const std::string& provider_name) const;
    std::vector<std::string> getAvailableProviders() const;
    std::string getDefaultProvider() const;
    int getTimeoutSeconds() const;
    int getRetryAttempts() const;
    int getRetryDelayMs() const;

private:
    APIConfigManager() = default;
    nlohmann::json config_;
    bool config_loaded_ = false;
};

} // namespace LLMEngineAPI
