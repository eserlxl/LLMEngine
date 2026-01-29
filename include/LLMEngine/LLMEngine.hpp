// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include "LLMEngine/APIClient.hpp"
#include "LLMEngine/AnalysisInput.hpp"
#include "LLMEngine/AnalysisResult.hpp"
#include "LLMEngine/IArtifactSink.hpp"
#include "LLMEngine/IConfigManager.hpp"
#include "LLMEngine/IMetricsCollector.hpp"
#include "LLMEngine/IModelContext.hpp"
#include "LLMEngine/IRequestExecutor.hpp"
#include "LLMEngine/ITempDirProvider.hpp"
#include "LLMEngine/LLMEngineExport.hpp"
#include "LLMEngine/Logger.hpp"
#include "LLMEngine/PromptBuilder.hpp"
#include "LLMEngine/RequestContext.hpp"
#include "LLMEngine/Constants.hpp"
#include "LLMEngine/RequestOptions.hpp"


#include <functional>
#include <future>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

namespace LLMEngine {

/**
 * @brief High-level interface for interacting with LLM providers.
 *
 * ## Thread Safety
 *
 * **LLMEngine instances ARE thread-safe** for `analyze` and `analyzeAsync` calls.
 * Internal state is protected by shared mutexes, allowing concurrent requests from
 * multiple threads on the same instance.
 *
 * Configuration methods (setters) are generally thread-safe via locking, but
 * consistent configuration should ideally be set during initialization.
 *
 * The underlying APIClient implementations are thread-safe (stateless).
 *
 * ## Lifecycle and Ownership
 *
 * - **LLMEngine instances** are moveable but not copyable
 * - **API Client ownership**: LLMEngine owns the APIClient instance via unique_ptr
 * - **Logger ownership**: LLMEngine holds a shared_ptr to the Logger, allowing
 *   multiple instances to share the same logger safely (if the logger is thread-safe)
 * - **Temp Directory Provider**: If provided, held via shared_ptr (shared ownership)
 *
 * ## Resource Management
 *
 * - Temporary directories are created per-request and cleaned up automatically
 * - Debug artifacts are managed according to retention policy
 * - No manual cleanup required for normal usage
 *
 * ## Example Usage
 *
 * ```cpp
 * // Single-threaded usage
 * LLMEngine engine(LLMEngineAPI::ProviderType::QWEN, apiKey, "qwen-flash");
 * auto result = engine.analyze("Analyze this code", input, "code_analysis");
 *
 * // Multi-threaded usage (create separate instances)
 * std::thread t1([&]() {
 *     LLMEngine engine1(LLMEngineAPI::ProviderType::QWEN, apiKey, "qwen-flash");
 *     // use engine1
 * });
 * std::thread t2([&]() {
 *     LLMEngine engine2(LLMEngineAPI::ProviderType::OPENAI, apiKey2, "gpt-4");
 *     // use engine2
 * });
 * ```
 */
class LLMENGINE_EXPORT LLMEngine : public IModelContext {
  public:
    // Constructor for API-based providers
    /**
     * @brief Construct an engine for an online provider.
     * @param providerType Provider enum value.
     * @param apiKey Provider API key.
     * @param model Default model name.
     * @param modelParams Default model params.
     * @param logRetentionHours Hours to keep debug artifacts.
     * @param debug Enable response artifact logging.
     */
    LLMEngine(::LLMEngineAPI::ProviderType providerType,
              std::string_view apiKey,
              std::string_view model,
              const nlohmann::json& modelParams = {},
              int logRetentionHours = Constants::DefaultValues::DEFAULT_LOG_RETENTION_HOURS,
              bool debug = false);

    // Constructor using config file
    /**
     * @brief Construct using provider name resolved via configuration.
     * @param providerName Provider key (e.g., "qwen").
     * @param apiKey Provider API key (optional if not required).
     * @param model Model name (optional, uses config default if empty).
     * @param modelParams Default model params.
     * @param logRetentionHours Hours to keep debug artifacts.
     * @param debug Enable response artifact logging.
     * @param configManager Optional configuration manager (shared ownership). If nullptr,
     *                       uses APIConfigManager::getInstance() singleton. This parameter
     *                       enables dependency injection for testing and custom configurations.
     */
    LLMEngine(std::string_view providerName,
              std::string_view apiKey = "",
              std::string_view model = "",
              const nlohmann::json& modelParams = {},
              int logRetentionHours = Constants::DefaultValues::DEFAULT_LOG_RETENTION_HOURS,
              bool debug = false,
              const std::shared_ptr<::LLMEngineAPI::IConfigManager>& configManager = nullptr,
              std::string_view baseUrl = "");

    // Dependency injection constructor for tests and advanced usage
    /**
     * @brief Construct with a custom API client (testing/advanced scenarios).
     *
     * **Ownership:** Takes ownership of the client via unique_ptr. The client
     * will be destroyed when this LLMEngine instance is destroyed.
     *
     * **Thread Safety:** The client implementation must be thread-safe if the
     * resulting LLMEngine instance will be used from multiple threads (though
     * LLMEngine itself is not thread-safe).
     *
     * @param client Custom API client instance (transferred ownership)
     * @param modelParams Default model params
     * @param logRetentionHours Hours to keep debug artifacts
     * @param debug Enable response artifact logging
     * @param tempDirProvider Optional temporary directory provider (shared ownership).
     *                          If nullptr, uses DefaultTempDirProvider. Must be
     *                          thread-safe if shared across multiple LLMEngine instances.
     */
    LLMEngine(std::unique_ptr<::LLMEngineAPI::APIClient> client,
              const nlohmann::json& modelParams = {},
              int logRetentionHours = Constants::DefaultValues::DEFAULT_LOG_RETENTION_HOURS,
              bool debug = false,
              const std::shared_ptr<ITempDirProvider>& tempDirProvider = nullptr);

    /**
     * @brief Run an analysis request.
     * @param prompt User/system instruction.
     * @param input Structured input payload.
     * @param analysisType Tag used for routing/processing.
     * @param mode Provider-specific mode (default: "chat").
     * @param prependTerseInstruction If true (default), prepends a system instruction asking for
     * brief, concise responses. Set to false to use the prompt verbatim without modification,
     * useful for evaluation or when precise prompt control is needed for downstream agents.
     * @return AnalysisResult with typed fields.
     *
     * @note By default, this method prepends the following instruction to your prompt:
     *       "Please respond directly to the previous message, engaging with its content.
     *        Try to be brief and concise and complete your response in one or two sentences,
     *        mostly one sentence.\n"
     *       To disable this behavior and use your prompt exactly as provided, set
     *       prependTerseInstruction to false.
     *
     * @example Basic usage:
     * ```cpp
     * LLMEngine engine(LLMEngineAPI::ProviderType::QWEN, apiKey, "qwen-flash");
     * nlohmann::json input = {{"system_prompt", "You are a helpful assistant."}};
     * auto result = engine.analyze("What is 2+2?", input, "math_question");
     * if (result.success) {
     *     std::cout << "Answer: " << result.content << std::endl;
     *     if (!result.think.empty()) {
     *         std::cout << "Reasoning: " << result.think << std::endl;
     *     }
     * } else {
     *     std::cerr << "Error: " << result.errorMessage << std::endl;
     * }
     * ```
     *
     * @example Error handling:
     * ```cpp
     * auto result = engine.analyze("Analyze this code", input, "code_analysis");
     * if (!result.success) {
     *     if (result.hasError(LLMEngineErrorCode::Auth)) {
     *         std::cerr << "Authentication failed" << std::endl;
     *     } else if (result.isRetriableError()) {
     *         // Retry the request
     *     }
     * }
     * ```
     *
     * @example With custom parameters:
     * ```cpp
     * nlohmann::json input = {
     *     {"system_prompt", "You are a code reviewer."},
     *     {"max_tokens", 500},
     *     {"temperature", 0.3}
     * };
     * auto result = engine.analyze("Review this code", input, "code_review", "chat", false);
     * ```
     */


    // Constructor for multi-threaded/async safety state
    struct EngineState;

    /**
     * @brief Run an analysis request with options.
     * @details Options provided here (e.g. timeout, headers) take precedence over 
     * global configuration/defaults.
     * @copydoc analyze
     */
    [[nodiscard]] AnalysisResult analyze(std::string_view prompt,
                                         const nlohmann::json& input,
                                         std::string_view analysisType,
                                         const RequestOptions& options);

    /**
     * @brief Run an analysis request using strongly-typed input.
     * @copydoc analyze
     */
    [[nodiscard]] AnalysisResult analyze(const AnalysisInput& input,
                                         std::string_view analysisType,
                                         const RequestOptions& options = {});

    // Overloaded analyze for backward compatibility
    [[nodiscard]] AnalysisResult analyze(std::string_view prompt,
                                         const nlohmann::json& input,
                                         std::string_view analysisType,
                                         std::string_view mode = "chat",
                                         bool prependTerseInstruction = true);

    /**
     * @brief Run an analysis request asynchronously.
     * @copydoc analyze
     * @return std::future<AnalysisResult> Future that will hold the result.
     */
    [[nodiscard]] std::future<AnalysisResult> analyzeAsync(std::string_view prompt,
                                                           const nlohmann::json& input,
                                                           std::string_view analysisType,
                                                           std::string_view mode = "chat",
                                                           bool prependTerseInstruction = true);

    /**
     * @brief Run an analysis request asynchronously with options.
     * @copydoc analyze
     */
    [[nodiscard]] std::future<AnalysisResult> analyzeAsync(std::string_view prompt,
                                                           const nlohmann::json& input,
                                                           std::string_view analysisType,
                                                           const RequestOptions& options);

    /**
     * @brief Run an analysis request with streaming response.
     *
     * @param callback Callback invoked for each chunk.
     */
    void analyzeStream(std::string_view prompt,
                       const nlohmann::json& input,
                       std::string_view analysisType,
                       const RequestOptions& options,
                       StreamCallback callback);

    /**
     * @brief Interceptor interface for request/response modification/inspection.
     */
    class IInterceptor {
      public:
        virtual ~IInterceptor() = default;
        /**
         * @brief Called before request execution.
         * @param ctx Read-write access to built request context.
         */
        virtual void onRequest(RequestContext& ctx) = 0;
        /**
         * @brief Called after response execution.
         * @param result Read-write access to analysis result.
         */
        virtual void onResponse(AnalysisResult& result) = 0;
    };

    /**
     * @brief Run a batch of analysis requests in parallel.
     *
     * @param inputs Vector of inputs to process.
     * @param analysisType Analysis type tag for all requests.
     * @param options Request options shared by all requests (unless overridden).
     * @return Vector of results in the same order as inputs.
     */
    [[nodiscard]] std::vector<AnalysisResult> analyzeBatch(const std::vector<AnalysisInput>& inputs,
                                                           std::string_view analysisType,
                                                           const RequestOptions& options = {});

    /**
     * @brief Add a request/response interceptor.
     * @param interceptor Shared pointer to interceptor.
     */
    void addInterceptor(std::shared_ptr<IInterceptor> interceptor);

    // Utility methods
    /** @brief Provider display name. */
    [[nodiscard]] std::string getProviderName() const;
    /** @brief Model name used by the engine. */
    [[nodiscard]] std::string getModelName() const;
    /** @brief Provider enumeration value. */
    [[nodiscard]] ::LLMEngineAPI::ProviderType getProviderType() const;
    /** @brief Backend type string (e.g., "ollama", "openai"). */
    [[nodiscard]] std::string getBackendType() const;
    /** @brief True if using an online provider (not local Ollama). */
    [[nodiscard]] bool isOnlineProvider() const;

    // IModelContext interface implementation
    [[nodiscard]] std::string getTempDirectory() const override;
    [[nodiscard]] std::shared_ptr<IPromptBuilder> getTersePromptBuilder() const override;
    [[nodiscard]] std::shared_ptr<IPromptBuilder> getPassthroughPromptBuilder() const override;
    [[nodiscard]] const nlohmann::json& getModelParams() const override;
    [[nodiscard]] bool areDebugFilesEnabled() const override;
    [[nodiscard]] std::shared_ptr<IArtifactSink> getArtifactSink() const override;
    [[nodiscard]] int getLogRetentionHours() const override;
    [[nodiscard]] std::shared_ptr<Logger> getLogger() const override;
    void prepareTempDirectory() override;

    // Additional accessors (not part of IModelContext)
    [[nodiscard]] bool isDebugEnabled() const;

    // Temporary directory configuration
    /**
     * @brief Set the temporary directory for debug artifacts.
     *
     * **Thread Safety:** This method modifies internal state and is not thread-safe.
     * Do not call concurrently with other methods on the same instance.
     *
     * **Security:** Only directories within the default root are accepted to prevent
     * accidental deletion of system directories.
     *
     * @param tmpDir Temporary directory path (must be within default root)
     * @return true if the directory was set successfully, false if it was rejected or an error
     * occurred
     */
    [[nodiscard]] bool setTempDirectory(const std::string& tmpDir);

    /**
     * @brief Set default request options applied to all requests unless overridden.
     * @param options Default options (timeout, retries, headers, etc.)
     */
    void setDefaultRequestOptions(const RequestOptions& options);

    // Logging
    /**
     * @brief Set a custom logger instance.
     *
     * **Ownership:** Takes shared ownership of the logger. Multiple LLMEngine instances
     * can share the same logger safely if the logger implementation is thread-safe.
     *
     * **Thread Safety:** The logger must be thread-safe if it will be used by multiple
     * LLMEngine instances concurrently. DefaultLogger is thread-safe.
     *
     * @param logger Logger instance (shared ownership)
     */
    void setLogger(std::shared_ptr<Logger> logger);
    /**
     * @brief Inject a policy to determine whether debug files should be written.
     *
     * If not set, the engine uses the cached value of LLMENGINE_DISABLE_DEBUG_FILES
     * read at construction time. Supplying a policy allows tests and long-lived
     * services to toggle behavior at runtime without rebuilding the engine.
     */
    void setDebugFilesPolicy(std::function<bool()> policy);

    /**
     * @brief Set whether debug files are enabled at runtime.
     *
     * This method allows runtime control over debug file writing, overriding
     * both the cached environment variable value and any injected policy.
     * Useful for dynamic toggling without environment variable changes.
     *
     * @param enabled True to enable debug files, false to disable
     */
    void setDebugFilesEnabled(bool enabled);

    /**
     * @brief Create a new cancellation token.
     * Use this factory instead of manually creating shared pointers.
     */
    static std::shared_ptr<CancellationToken> createCancellationToken();

    // Dependency injection setters
    /** @brief Replace the request executor strategy. */
    void setRequestExecutor(std::shared_ptr<IRequestExecutor> executor);
    /** @brief Replace the artifact sink strategy. */
    void setArtifactSink(std::shared_ptr<IArtifactSink> sink);
    /** @brief Replace prompt builders. Null keeps existing. */
    void setPromptBuilders(std::shared_ptr<IPromptBuilder> terse,
                           std::shared_ptr<IPromptBuilder> passthrough);

    /** @brief Set metrics collector. */
    void setMetricsCollector(std::shared_ptr<IMetricsCollector> collector);

  private:
    void initializeAPIClient();
    void ensureSecureTmpDir();
    void validateInput(const AnalysisInput& input) const;

    // Internal state pointer (PIMPL pattern + Shared Ownership for Async)
    std::shared_ptr<EngineState> state_;
};

} // namespace LLMEngine
