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
#include <vector>
#include <memory>
#include "LLMEngine/LLMEngineExport.hpp"
#include "LLMEngine/APIClient.hpp"
#include "LLMEngine/Logger.hpp"
#include "LLMEngine/ITempDirProvider.hpp"

namespace LLMEngine {

struct LLMENGINE_EXPORT AnalysisResult {
    bool success;
    std::string think;
    std::string content;
    std::string errorMessage;
    int statusCode;
};

/**
 * @brief High-level interface for interacting with LLM providers.
 * 
 * ## Thread Safety
 * 
 * **LLMEngine instances are NOT thread-safe.** Each instance should be used from
 * a single thread. To use LLMEngine from multiple threads, create separate instances.
 * 
 * The underlying APIClient implementations are thread-safe (stateless), but the
 * LLMEngine class maintains internal state (model parameters, configuration, logger)
 * that is not protected by locks.
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
 * LLMEngine engine(LLMEngineAPI::ProviderType::QWEN, api_key, "qwen-flash");
 * auto result = engine.analyze("Analyze this code", input, "code_analysis");
 * 
 * // Multi-threaded usage (create separate instances)
 * std::thread t1([&]() {
 *     LLMEngine engine1(LLMEngineAPI::ProviderType::QWEN, api_key, "qwen-flash");
 *     // use engine1
 * });
 * std::thread t2([&]() {
 *     LLMEngine engine2(LLMEngineAPI::ProviderType::OPENAI, api_key2, "gpt-4");
 *     // use engine2
 * });
 * ```
 */
class LLMENGINE_EXPORT LLMEngine {
public:
    // Constructor for API-based providers
    /**
     * @brief Construct an engine for an online provider.
     * @param provider_type Provider enum value.
     * @param api_key Provider API key.
     * @param model Default model name.
     * @param model_params Default model params.
     * @param log_retention_hours Hours to keep debug artifacts.
     * @param debug Enable response artifact logging.
     */
    LLMEngine(::LLMEngineAPI::ProviderType provider_type, std::string_view api_key, std::string_view model, const nlohmann::json& model_params = {}, int log_retention_hours = 24, bool debug = false);
    
    // Constructor using config file
    /**
     * @brief Construct using provider name resolved via configuration.
     * @param provider_name Provider key (e.g., "qwen").
     * @param api_key Provider API key (optional if not required).
     * @param model Model name (optional, uses config default if empty).
     * @param model_params Default model params.
     * @param log_retention_hours Hours to keep debug artifacts.
     * @param debug Enable response artifact logging.
     */
    LLMEngine(std::string_view provider_name, std::string_view api_key = "", std::string_view model = "", const nlohmann::json& model_params = {}, int log_retention_hours = 24, bool debug = false);
    
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
     * @param model_params Default model params
     * @param log_retention_hours Hours to keep debug artifacts
     * @param debug Enable response artifact logging
     * @param temp_dir_provider Optional temporary directory provider (shared ownership).
     *                          If nullptr, uses DefaultTempDirProvider. Must be
     *                          thread-safe if shared across multiple LLMEngine instances.
     */
    LLMEngine(std::unique_ptr<::LLMEngineAPI::APIClient> client,
              const nlohmann::json& model_params = {},
              int log_retention_hours = 24,
              bool debug = false,
              std::shared_ptr<ITempDirProvider> temp_dir_provider = nullptr);
    
    /**
     * @brief Run an analysis request.
     * @param prompt User/system instruction.
     * @param input Structured input payload.
     * @param analysis_type Tag used for routing/processing.
     * @param mode Provider-specific mode (default: "chat").
     * @param prepend_terse_instruction If true (default), prepends a system instruction asking for brief, concise responses. 
     *                                  Set to false to use the prompt verbatim without modification, useful for evaluation 
     *                                  or when precise prompt control is needed for downstream agents.
     * @return AnalysisResult with typed fields.
     * 
     * @note By default, this method prepends the following instruction to your prompt:
     *       "Please respond directly to the previous message, engaging with its content. 
     *        Try to be brief and concise and complete your response in one or two sentences, 
     *        mostly one sentence.\n"
     *       To disable this behavior and use your prompt exactly as provided, set 
     *       prepend_terse_instruction to false.
     */
    [[nodiscard]] AnalysisResult analyze(std::string_view prompt, const nlohmann::json& input, std::string_view analysis_type, std::string_view mode = "chat", bool prepend_terse_instruction = true) const;
    
    // Utility methods
    /** @brief Provider display name. */
    [[nodiscard]] std::string getProviderName() const;
    /** @brief Model name used by the engine. */
    [[nodiscard]] std::string getModelName() const;
    /** @brief Provider enumeration value. */
    [[nodiscard]] ::LLMEngineAPI::ProviderType getProviderType() const;
    /** @brief True if using an online provider (not local Ollama). */
    [[nodiscard]] bool isOnlineProvider() const;
    
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
     * @param tmp_dir Temporary directory path (must be within default root)
     */
    void setTempDirectory(const std::string& tmp_dir);
    
    /**
     * @brief Get the current temporary directory path.
     * 
     * **Thread Safety:** This method is thread-safe for read access, but LLMEngine
     * itself is not thread-safe, so concurrent access should be avoided.
     * 
     * @return Current temporary directory path
     */
    [[nodiscard]] std::string getTempDirectory() const;
    
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
    
private:
    void cleanupResponseFiles() const;
    void initializeAPIClient();
    void ensureSecureTmpDir() const;
    
    std::string model_;
    nlohmann::json model_params_;
    int log_retention_hours_;
    bool debug_;
    std::string tmp_dir_;  // Configurable temporary directory (defaults to Utils::TMP_DIR)
    
    // API client support
    std::unique_ptr<::LLMEngineAPI::APIClient> api_client_;
    ::LLMEngineAPI::ProviderType provider_type_;
    std::string api_key_;
    std::string ollama_url_;  // Only used when provider_type is OLLAMA
    std::shared_ptr<Logger> logger_;
};

} // namespace LLMEngine
