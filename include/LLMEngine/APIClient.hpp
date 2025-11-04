// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include <string>
#include <string_view>
#include <nlohmann/json.hpp>
#include <memory>
#include <map>
#include <vector>
#include <shared_mutex>

// Ensure export macros are available
#include "LLMEngine/LLMEngineExport.hpp"

namespace LLMEngineAPI {

/**
 * @brief Supported LLM providers.
 */
enum class ProviderType {
    QWEN,
    OPENAI,
    ANTHROPIC,
    OLLAMA,
    GEMINI
};

/**
 * @brief Normalized API response returned by all providers.
 */
struct APIResponse {
    bool success;
    std::string content;
    std::string error_message;
    int status_code;
    nlohmann::json raw_response;
    enum class APIError {
        None,
        Network,
        Timeout,
        InvalidResponse,
        Auth,
        RateLimited,
        Server,
        Unknown
    } error_code = APIError::None;
};

/**
 * @brief Abstract client interface implemented by all providers.
 */
class LLMENGINE_EXPORT APIClient {
public:
    virtual ~APIClient() = default;
    /**
     * @brief Send a text generation request.
     * @param prompt User prompt or system instruction.
     * @param input Additional structured inputs (tool/context).
     * @param params Model parameters (temperature, max_tokens, etc.).
     * @return Provider-agnostic APIResponse.
     */
    virtual APIResponse sendRequest(std::string_view prompt, 
                                   const nlohmann::json& input,
                                   const nlohmann::json& params) const = 0;
    /** @brief Human-readable provider name. */
    virtual std::string getProviderName() const = 0;
    /** @brief Provider enumeration value. */
    virtual ProviderType getProviderType() const = 0;
};

/**
 * @brief Qwen (DashScope) client.
 */
class LLMENGINE_EXPORT QwenClient : public APIClient {
public:
    /**
     * @param api_key Qwen API key (environment-retrieved recommended).
     * @param model Default model, e.g. "qwen-flash".
     */
    QwenClient(const std::string& api_key, const std::string& model = "qwen-flash");
    APIResponse sendRequest(std::string_view prompt, 
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

/**
 * @brief OpenAI client.
 */
class LLMENGINE_EXPORT OpenAIClient : public APIClient {
public:
    /**
     * @param api_key OpenAI API key.
     * @param model Default model, e.g. "gpt-3.5-turbo".
     */
    OpenAIClient(const std::string& api_key, const std::string& model = "gpt-3.5-turbo");
    APIResponse sendRequest(std::string_view prompt, 
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

/**
 * @brief Anthropic Claude client.
 */
class LLMENGINE_EXPORT AnthropicClient : public APIClient {
public:
    /**
     * @param api_key Anthropic API key.
     * @param model Default model, e.g. "claude-3-sonnet".
     */
    AnthropicClient(const std::string& api_key, const std::string& model = "claude-3-sonnet");
    APIResponse sendRequest(std::string_view prompt, 
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

/**
 * @brief Local Ollama client.
 */
class LLMENGINE_EXPORT OllamaClient : public APIClient {
public:
    /**
     * @param base_url Ollama server URL.
     * @param model Default local model name.
     */
    OllamaClient(const std::string& base_url = "http://localhost:11434", 
                 const std::string& model = "llama2");
    APIResponse sendRequest(std::string_view prompt, 
                           const nlohmann::json& input,
                           const nlohmann::json& params) const override;
    std::string getProviderName() const override { return "Ollama"; }
    ProviderType getProviderType() const override { return ProviderType::OLLAMA; }

private:
    std::string base_url_;
    std::string model_;
    nlohmann::json default_params_;
};

/**
 * @brief Google Gemini (AI Studio) client.
 */
class LLMENGINE_EXPORT GeminiClient : public APIClient {
public:
    /**
     * @param api_key Google AI Studio API key.
     * @param model Default model, e.g. "gemini-1.5-flash".
     */
    GeminiClient(const std::string& api_key, const std::string& model = "gemini-1.5-flash");
    APIResponse sendRequest(std::string_view prompt,
                           const nlohmann::json& input,
                           const nlohmann::json& params) const override;
    std::string getProviderName() const override { return "Gemini"; }
    ProviderType getProviderType() const override { return ProviderType::GEMINI; }

private:
    std::string api_key_;
    std::string model_;
    std::string base_url_;
    nlohmann::json default_params_;
};

/**
 * @brief Factory for creating provider clients.
 */
class LLMENGINE_EXPORT APIClientFactory {
public:
    /**
     * @brief Create client by enum type.
     */
    static std::unique_ptr<APIClient> createClient(ProviderType type, 
                                                   std::string_view api_key = "",
                                                   std::string_view model = "",
                                                   std::string_view base_url = "");
    
    /**
     * @brief Create client by provider name from JSON config.
     */
    static std::unique_ptr<APIClient> createClientFromConfig(std::string_view provider_name,
                                                             const nlohmann::json& config);
    
    /**
     * @brief Convert provider string to enum.
     */
    static ProviderType stringToProviderType(std::string_view provider_name);
    /**
     * @brief Convert enum to provider string.
     */
    static std::string providerTypeToString(ProviderType type);
};

/**
 * @brief Singleton managing `api_config.json` loading and access.
 * 
 * Thread-safe singleton for configuration management. All methods are thread-safe
 * and can be called concurrently from multiple threads.
 */
class LLMENGINE_EXPORT APIConfigManager {
public:
    static APIConfigManager& getInstance();
    
    /**
     * @brief Set the default configuration file path.
     * @param config_path The path to use as default when loadConfig() is called without arguments.
     */
    void setDefaultConfigPath(std::string_view config_path);
    /**
     * @brief Get the current default configuration file path.
     * @return The current default config path.
     */
    [[nodiscard]] std::string getDefaultConfigPath() const;
    /**
     * @brief Load configuration from path or default search order.
     * @param config_path Optional explicit path. If empty, uses the default path set via setDefaultConfigPath().
     * @return true if configuration loaded successfully.
     */
    bool loadConfig(std::string_view config_path = "");
    /** @brief Get provider-specific JSON config. */
    [[nodiscard]] nlohmann::json getProviderConfig(std::string_view provider_name) const;
    /** @brief List all available provider keys. */
    [[nodiscard]] std::vector<std::string> getAvailableProviders() const;
    /** @brief Default provider key. */
    [[nodiscard]] std::string getDefaultProvider() const;
    /** @brief Global timeout seconds. */
    [[nodiscard]] int getTimeoutSeconds() const;
    /** @brief Get timeout seconds for a specific provider, falling back to global if not provider-specific. */
    [[nodiscard]] int getTimeoutSeconds(std::string_view provider_name) const;
    /** @brief Global retry attempts. */
    [[nodiscard]] int getRetryAttempts() const;
    /** @brief Delay in milliseconds between retries. */
    [[nodiscard]] int getRetryDelayMs() const;

private:
    APIConfigManager();
    mutable std::shared_mutex mutex_;  // For read-write lock
    nlohmann::json config_;
    bool config_loaded_ = false;
    std::string default_config_path_;  // Default config file path
};

} // namespace LLMEngineAPI


