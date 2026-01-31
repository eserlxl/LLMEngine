// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

// Ensure export macros are available
#include "LLMEngine/core/AnalysisResult.hpp"
#include "LLMEngine/core/ErrorCodes.hpp"
#include "LLMEngine/core/IConfigManager.hpp"
#include "LLMEngine/LLMEngineExport.hpp"
#include "LLMEngine/utils/Logger.hpp"
#include "LLMEngine/http/RequestOptions.hpp"

#include <functional>

namespace LLMEngineAPI {

/**
 * @brief Supported LLM providers.
 *
 * **Thread Safety:** This enum is thread-safe (enum values are immutable).
 */
enum class ProviderType { QWEN, OPENAI, ANTHROPIC, OLLAMA, GEMINI };

/**
 * @brief Normalized API response returned by all providers.
 *
 * **Thread Safety:** This struct is thread-safe for read access. It is typically
 * created by APIClient implementations and returned by value.
 *
 * **Ownership:** Contains a nlohmann::json object (value semantics, no special
 * ownership concerns).
 */
struct APIResponse {
    bool success;
    std::string content;
    std::string errorMessage;
    int statusCode;
    nlohmann::json rawResponse;

    // Use unified error code enum
    // Backward compatibility: APIError is an alias for LLMEngineErrorCode
    using APIError = ::LLMEngine::LLMEngineErrorCode;
    LLMEngine::LLMEngineErrorCode errorCode = LLMEngine::LLMEngineErrorCode::None;

    // Token usage statistics
    LLMEngine::AnalysisResult::UsageStats usage;

    // Reason for completion (stop, length, tool_calls, content_filter, etc.)
    std::string finishReason;
};

/**
 * @brief Abstract client interface implemented by all providers.
 *
 * ## Thread Safety
 *
 * **APIClient implementations are thread-safe.** They are stateless (except for
 * configuration set during construction) and can be called concurrently from
 * multiple threads. However, each instance should be used with a single provider
 * configuration.
 *
 * ## Lifecycle and Ownership
 *
 * - **Construction:** Clients are created via APIClientFactory
 * - **Ownership:** Typically owned by LLMEngine via unique_ptr
 * - **Lifetime:** Clients live as long as the owning LLMEngine instance
 * - **Thread Safety:** Stateless design allows concurrent sendRequest() calls
 *
 * ## Implementation Requirements
 *
 * Derived classes must:
 * - Be thread-safe (stateless or properly synchronized)
 * - Handle errors gracefully and return appropriate APIResponse
 * - Implement retry logic or rely on shared retry helpers
 * - Provide thread-safe access to configuration
 */
class LLMENGINE_EXPORT APIClient {
  public:
    virtual ~APIClient() = default;

    // ... (skipping namespace check, assumes context)

    /**
     * @brief Send a text generation request.
     * @param prompt User prompt or system instruction.
     * @param input Additional structured inputs (tool/context).
     * @param params Model parameters (temperature, max_tokens, etc.).
     * @param options Per-request options (timeout, cancellation, etc.).
     * @return Provider-agnostic APIResponse.
     */
    virtual APIResponse sendRequest(std::string_view prompt,
                                    const nlohmann::json& input,
                                    const nlohmann::json& params,
                                    const ::LLMEngine::RequestOptions& options = {}) const = 0;

    /**
     * @brief Sends a streaming text generation request.
     * @param prompt User prompt or system instruction.
     * @param input Additional structured inputs.
     * @param params Model parameters.
     * @param callback Callback invoked for each text chunk received.
     * @param options Per-request options (timeout, cancellation, etc.).
     * @throws std::runtime_error if not implemented or on failure.
     */
    virtual void sendRequestStream(std::string_view prompt,
                                   const nlohmann::json& input,
                                   const nlohmann::json& params,
                                   LLMEngine::StreamCallback callback,
                                   const ::LLMEngine::RequestOptions& options = {}) const {
        (void)prompt;
        (void)input;
        (void)params;
        (void)callback;
        (void)options;
        throw std::runtime_error("Streaming not implemented for this provider");
    }
    /** @brief Human-readable provider name. */
    virtual std::string getProviderName() const = 0;
    /** @brief Provider enumeration value. */
    virtual ProviderType getProviderType() const = 0;
    /** @brief Optionally inject configuration manager (fallback to singleton when not set). */
    virtual void setConfig(std::shared_ptr<IConfigManager>) {}
};

// Forward declaration for OpenAICompatibleClient (implementation in src/)
// Note: OpenAICompatibleClient is an internal implementation detail
// QwenClient and OpenAIClient inherit from it but expose APIClient interface

/**
 * @brief Qwen (DashScope) client.
 *
 * Uses OpenAICompatibleClient internally to share common OpenAI-compatible API logic.
 */
class LLMENGINE_EXPORT QwenClient : public APIClient {
  public:
    /**
     * @param api_key Qwen API key (environment-retrieved recommended).
     * @param model Default model, e.g. "qwen-flash".
     */
    QwenClient(const std::string& api_key, const std::string& model = "qwen-flash");
    ~QwenClient();
    APIResponse sendRequest(std::string_view prompt,
                            const nlohmann::json& input,
                            const nlohmann::json& params,
                            const ::LLMEngine::RequestOptions& options = {}) const override;
    void sendRequestStream(std::string_view prompt,
                           const nlohmann::json& input,
                           const nlohmann::json& params,
                           LLMEngine::StreamCallback callback,
                           const ::LLMEngine::RequestOptions& options = {}) const override;
    std::string getProviderName() const override {
        return "Qwen";
    }
    ProviderType getProviderType() const override {
        return ProviderType::QWEN;
    }
    void setConfig(std::shared_ptr<IConfigManager> cfg) override;
    
  protected:
    // Exposed for testing
    nlohmann::json buildPayload(std::string_view prompt, const nlohmann::json& input, const nlohmann::json& request_params) const;
    std::string buildUrl() const;
    std::map<std::string, std::string> buildHeaders() const;

  private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief OpenAI client.
 *
 * Uses OpenAICompatibleClient internally to share common OpenAI-compatible API logic.
 */
class LLMENGINE_EXPORT OpenAIClient : public APIClient {
  public:
    /**
     * @param api_key OpenAI API key.
     * @param model Default model, e.g. "gpt-3.5-turbo".
     */
    OpenAIClient(const std::string& api_key, const std::string& model = "gpt-3.5-turbo");
    ~OpenAIClient();
    APIResponse sendRequest(std::string_view prompt,
                            const nlohmann::json& input,
                            const nlohmann::json& params,
                            const ::LLMEngine::RequestOptions& options = {}) const override;
    void sendRequestStream(std::string_view prompt,
                           const nlohmann::json& input,
                           const nlohmann::json& params,
                           LLMEngine::StreamCallback callback,
                           const ::LLMEngine::RequestOptions& options = {}) const override;
    std::string getProviderName() const override {
        return "OpenAI";
    }
    ProviderType getProviderType() const override {
        return ProviderType::OPENAI;
    }
    void setConfig(std::shared_ptr<IConfigManager> cfg) override;

  protected:
    // Exposed for testing
    nlohmann::json buildPayload(std::string_view prompt, const nlohmann::json& input, const nlohmann::json& request_params) const;
    std::string buildUrl() const;
    std::map<std::string, std::string> buildHeaders() const;

  private:
    class Impl;
    std::unique_ptr<Impl> impl_;
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
                            const nlohmann::json& params,
                            const ::LLMEngine::RequestOptions& options = {}) const override;
    std::string getProviderName() const override {
        return "Anthropic";
    }
    ProviderType getProviderType() const override {
        return ProviderType::ANTHROPIC;
    }
    void sendRequestStream(std::string_view prompt,
                           const nlohmann::json& input,
                           const nlohmann::json& params,
                           LLMEngine::StreamCallback callback,
                           const ::LLMEngine::RequestOptions& options = {}) const override;
    void setConfig(std::shared_ptr<IConfigManager> cfg) override {
        config_ = std::move(cfg);
    }

  protected:
    // Exposed for testing
    nlohmann::json buildPayload(std::string_view prompt, const nlohmann::json& input, const nlohmann::json& request_params) const;
    std::string buildUrl() const;
    std::map<std::string, std::string> buildHeaders() const;

  private:
    std::string apiKey_;
    std::string model_;
    std::string baseUrl_;
    nlohmann::json defaultParams_;
    std::shared_ptr<IConfigManager> config_;
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
                            const nlohmann::json& params,
                            const ::LLMEngine::RequestOptions& options = {}) const override;
    std::string getProviderName() const override {
        return "Ollama";
    }
    ProviderType getProviderType() const override {
        return ProviderType::OLLAMA;
    }
    void sendRequestStream(std::string_view prompt,
                           const nlohmann::json& input,
                           const nlohmann::json& params,
                           LLMEngine::StreamCallback callback,
                           const ::LLMEngine::RequestOptions& options = {}) const override;
    void setConfig(std::shared_ptr<IConfigManager> cfg) override {
        config_ = std::move(cfg);
    }

  protected:
    // Exposed for testing
    nlohmann::json buildPayload(std::string_view prompt, const nlohmann::json& input, const nlohmann::json& request_params, bool stream) const;
    std::string buildUrl(bool use_generate) const;
    std::map<std::string, std::string> buildHeaders() const;

  private:
    std::string baseUrl_;
    std::string model_;
    nlohmann::json defaultParams_;
    std::shared_ptr<IConfigManager> config_;
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
                            const nlohmann::json& params,
                            const ::LLMEngine::RequestOptions& options = {}) const override;
    std::string getProviderName() const override {
        return "Gemini";
    }
    ProviderType getProviderType() const override {
        return ProviderType::GEMINI;
    }
    void sendRequestStream(std::string_view prompt,
                           const nlohmann::json& input,
                           const nlohmann::json& params,
                           LLMEngine::StreamCallback callback,
                           const ::LLMEngine::RequestOptions& options = {}) const override;
    void setConfig(std::shared_ptr<IConfigManager> cfg) override {
        config_ = std::move(cfg);
    }

  protected:
    // Exposed for testing
    nlohmann::json buildPayload(std::string_view prompt, const nlohmann::json& input, const nlohmann::json& request_params) const;
    std::string buildUrl(bool stream) const;
    std::map<std::string, std::string> buildHeaders() const;

  private:
    std::string apiKey_;
    std::string model_;
    std::string baseUrl_;
    nlohmann::json defaultParams_;
    std::shared_ptr<IConfigManager> config_;
};

/**
 * @brief Factory for creating provider clients.
 *
 * ## Thread Safety
 *
 * **All factory methods are thread-safe.** They are stateless and can be called
 * concurrently from multiple threads.
 *
 * ## Ownership
 *
 * Factory methods return unique_ptr, transferring ownership to the caller.
 * The caller is responsible for managing the lifetime of the created client.
 *
 * ## Credential Resolution
 *
 * Credentials are resolved in the following order:
 * 1. Environment variables (highest priority)
 * 2. Configuration file (with security warning)
 * 3. Exception if required and not found
 */
class LLMENGINE_EXPORT APIClientFactory {
  public:
    /**
     * @brief Create client by enum type.
     */
    static std::unique_ptr<APIClient> createClient(
        ProviderType type,
        std::string_view api_key = "",
        std::string_view model = "",
        std::string_view base_url = "",
        const std::shared_ptr<IConfigManager>& cfg = nullptr);

    /**
     * @brief Create client by provider name from JSON config.
     * @param provider_name Provider name (e.g., "qwen", "openai")
     * @param config Provider configuration JSON
     * @param logger Optional logger for warnings and errors (nullptr to suppress)
     */
    static std::unique_ptr<APIClient> createClientFromConfig(
        std::string_view provider_name,
        const nlohmann::json& config,
        ::LLMEngine::Logger* logger = nullptr,
        const std::shared_ptr<IConfigManager>& cfg = nullptr);

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
 * Implements IConfigManager interface for dependency injection support while
 * maintaining singleton pattern for backward compatibility.
 *
 * ## Thread Safety
 *
 * **All methods are thread-safe** and can be called concurrently from multiple threads.
 * Uses reader-writer locks (std::shared_mutex) to allow concurrent reads while
 * protecting writes.
 *
 * ## Lifecycle
 *
 * - **Singleton Pattern:** Single instance per process (created on first getInstance() call)
 * - **Lifetime:** Lives for the duration of the program
 * - **Initialization:** Config is loaded lazily via loadConfig()
 *
 * ## Ownership
 *
 * - **Logger:** Holds a weak_ptr to avoid dangling pointer risks. If the logger
 *   is destroyed, the singleton falls back to a thread-safe DefaultLogger.
 * - **Configuration:** Owned by the singleton instance
 * - **Thread Safety:** Logger access is protected by mutex
 *
 * ## Usage Pattern
 *
 * ```cpp
 * // Singleton usage (backward compatible)
 * auto& config = APIConfigManager::getInstance();
 * config.loadConfig();  // Load from default path
 * auto provider_config = config.getProviderConfig("qwen");
 *
 * // Dependency injection usage (new)
 * std::shared_ptr<IConfigManager> config = std::make_shared<APIConfigManager>();
 * // or use singleton as shared_ptr
 * auto config = std::shared_ptr<IConfigManager>(&APIConfigManager::getInstance(), [](auto*) {});
 * ```
 */
class LLMENGINE_EXPORT APIConfigManager : public IConfigManager {
  public:
    static APIConfigManager& getInstance();

    /**
     * @brief Set the default configuration file path.
     * @param config_path The path to use as default when loadConfig() is called without arguments.
     */
    void setDefaultConfigPath(std::string_view config_path) override;
    /**
     * @brief Get the current default configuration file path.
     * @return The current default config path.
     */
    [[nodiscard]] std::string getDefaultConfigPath() const override;
    void setLogger(std::shared_ptr<::LLMEngine::Logger> logger) override;
    bool loadConfig(std::string_view config_path = "") override;
    [[nodiscard]] nlohmann::json getProviderConfig(std::string_view provider_name) const override;
    [[nodiscard]] std::vector<std::string> getAvailableProviders() const override;
    [[nodiscard]] std::string getDefaultProvider() const override;
    [[nodiscard]] int getTimeoutSeconds() const override;
    [[nodiscard]] int getTimeoutSeconds(std::string_view provider_name) const override;
    [[nodiscard]] int getRetryAttempts() const override;
    [[nodiscard]] int getRetryDelayMs() const override;

  private:
    APIConfigManager();
    mutable std::shared_mutex mutex_; // For read-write lock
    nlohmann::json config_;
    bool config_loaded_ = false;
    std::string default_config_path_; // Default config file path
    std::weak_ptr<::LLMEngine::Logger>
        logger_; // Optional logger (weak ownership to avoid dangling pointers)

    // Helper method to safely get logger (locks weak_ptr or returns DefaultLogger)
    std::shared_ptr<::LLMEngine::Logger> getLogger() const;
};

} // namespace LLMEngineAPI
