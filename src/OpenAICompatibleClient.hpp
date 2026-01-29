// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include "APIClientCommon.hpp"
#include "ChatCompletionRequestHelper.hpp"
#include "LLMEngine/APIClient.hpp"
#include "LLMEngine/AnalysisResult.hpp"
#include "LLMEngine/Constants.hpp"
#include "LLMEngine/RequestOptions.hpp"

#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

namespace LLMEngineAPI {

/**
 * @brief Base class for OpenAI-compatible API clients.
 *
 * This class provides a common implementation for providers that use the
 * OpenAI-compatible API format (e.g., Qwen, OpenAI, and other compatible providers).
 * It handles payload building, URL construction, header building, and response parsing.
 *
 * ## Thread Safety
 *
 * This class is thread-safe. It is stateless (except for configuration set during
 * construction) and can be called concurrently from multiple threads.
 */
// Note: This class is not meant to be used directly as an APIClient.
// It provides common functionality for OpenAI-compatible clients but
// does not implement the pure virtual functions from APIClient.
// Wrapper classes (QwenClient, OpenAIClient) use this via PIMPL pattern.
class OpenAICompatibleClient {
  protected:
    /**
     * @brief Construct an OpenAI-compatible client.
     * @param api_key API key for authentication
     * @param model Default model name
     * @param base_url Base URL for the API endpoint
     */
    OpenAICompatibleClient(const std::string& api_key,
                           const std::string& model,
                           const std::string& base_url);

  public:
    void setConfig(std::shared_ptr<IConfigManager> cfg) {
        config_ = std::move(cfg);
    }

    /**
     * @brief Get cached headers (built once in constructor).
     * @return Header map with Content-Type and Authorization
     */
    const std::map<std::string, std::string>& getHeaders() const;

    /**
     * @brief Build the chat completions endpoint URL.
     * @return Full URL string
     */
    std::string buildUrl() const;

    /**
     * @brief Build the request payload.
     * @param messages Messages array (from ChatMessageBuilder)
     * @param request_params Merged request parameters
     * @return JSON payload object
     */
    nlohmann::json buildPayload(const nlohmann::json& messages,
                                const nlohmann::json& request_params) const;

    /**
     * @brief Parse OpenAI-compatible response format.
     * @param response Response object to populate
     * @param raw_text Raw response text (unused, kept for interface compatibility)
     */
    static void parseOpenAIResponse(APIResponse& response, const std::string& raw_text);

    /**
     * @brief Parse a chunk of OpenAI-compatible streaming response.
     * @param chunk Raw text chunk (part of SSE stream).
     * @param buffer Buffer for accumulating partial lines (updated in place).
     * @param callback User callback to receive extracted content.
     */
    static void parseOpenAIStreamChunk(std::string_view chunk,
                                       std::string& buffer,
                                       LLMEngine::StreamCallback callback);

    /**
     * @brief Send a streaming request (helper for wrapper classes).
     */
    void sendRequestStream(std::string_view prompt,
                           const nlohmann::json& input,
                           const nlohmann::json& params,
                           LLMEngine::StreamCallback callback,
                           const ::LLMEngine::RequestOptions& options = {});

    /**
     * @brief Get default parameters (for use by wrapper classes).
     */
    const nlohmann::json& getDefaultParams() const {
        return defaultParams_;
    }

    /**
     * @brief Get config manager (for use by wrapper classes).
     */
    IConfigManager* getConfig() const {
        return config_.get();
    }

  protected:
    std::string apiKey_;
    std::string model_;
    std::string baseUrl_;
    std::string chatCompletionsUrl_;                  // Cached URL
    std::map<std::string, std::string> cachedHeaders_; // Cached headers
    nlohmann::json defaultParams_;
    std::shared_ptr<IConfigManager> config_;
};

} // namespace LLMEngineAPI
