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
     */
    LLMEngine(std::unique_ptr<::LLMEngineAPI::APIClient> client,
              const nlohmann::json& model_params = {},
              int log_retention_hours = 24,
              bool debug = false);
    
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
    void setTempDirectory(const std::string& tmp_dir);
    [[nodiscard]] std::string getTempDirectory() const;
    // Logging
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
