// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include <vector>
#include <memory>
#include "APIClient.hpp"

/**
 * @brief High-level interface for interacting with LLM providers.
 */
class LLMEngine {
public:
    // Legacy constructor for Ollama (backward compatibility)
    /**
     * @brief Construct an engine for local Ollama.
     * @param ollama_url Base URL of Ollama server.
     * @param model Default model name.
     * @param model_params Default model params (temperature, max_tokens, ...).
     * @param log_retention_hours Hours to keep debug artifacts.
     * @param debug Enable response artifact logging.
     */
    LLMEngine(const std::string& ollama_url, const std::string& model, const nlohmann::json& model_params = {}, int log_retention_hours = 24, bool debug = false);
    
    // New constructor for API-based providers
    /**
     * @brief Construct an engine for an online provider.
     * @param provider_type Provider enum value.
     * @param api_key Provider API key.
     * @param model Default model name.
     * @param model_params Default model params.
     * @param log_retention_hours Hours to keep debug artifacts.
     * @param debug Enable response artifact logging.
     */
    LLMEngine(::LLMEngineAPI::ProviderType provider_type, const std::string& api_key, const std::string& model, const nlohmann::json& model_params = {}, int log_retention_hours = 24, bool debug = false);
    
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
    LLMEngine(const std::string& provider_name, const std::string& api_key = "", const std::string& model = "", const nlohmann::json& model_params = {}, int log_retention_hours = 24, bool debug = false);
    
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
     * @return Vector of strings: [raw_json, primary_text, ...].
     */
    std::vector<std::string> analyze(const std::string& prompt, const nlohmann::json& input, const std::string& analysis_type, const std::string& mode = "chat") const;
    
    // Utility methods
    /** @brief Provider display name. */
    std::string getProviderName() const;
    /** @brief Model name used by the engine. */
    std::string getModelName() const;
    /** @brief Provider enumeration value. */
    ::LLMEngineAPI::ProviderType getProviderType() const;
    /** @brief True if using an online provider (not local Ollama). */
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