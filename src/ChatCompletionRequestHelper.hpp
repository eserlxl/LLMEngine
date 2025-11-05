// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include "LLMEngine/APIClient.hpp"
#include "APIClientCommon.hpp"
#include "LLMEngine/Constants.hpp"
#include <nlohmann/json.hpp>
#include <cpr/cpr.h>
#include <string>
#include <string_view>
#include <map>

namespace LLMEngineAPI {

/**
 * @brief Helper for building chat completion messages array.
 * 
 * Common pattern: build messages array with optional system prompt and user message.
 */
struct ChatMessageBuilder {
    static nlohmann::json buildMessages(std::string_view prompt, const nlohmann::json& input) {
        nlohmann::json messages = nlohmann::json::array();
        
        // Add system message if input contains system prompt
        if (input.contains(std::string(::LLMEngine::Constants::JsonKeys::SYSTEM_PROMPT))) {
            const auto& sp = input.at(std::string(::LLMEngine::Constants::JsonKeys::SYSTEM_PROMPT));
            if (sp.is_string()) {
                messages.push_back({{"role", "system"}, {"content", sp.get<std::string>()}});
            } else if (sp.is_number() || sp.is_boolean()) {
                messages.push_back({{"role", "system"}, {"content", sp.dump()}});
            } else {
                // Ignore non-string non-scalar system_prompt to avoid exceptions
            }
        }
        
        // Add user message
        messages.push_back({
            {"role", "user"},
            {"content", prompt}
        });
        
        return messages;
    }
};

/**
 * @brief Helper for common chat completion request lifecycle.
 * 
 * Encapsulates the common pattern: merge params, compute retry settings,
 * get timeout, execute request with retries. Provider-specific parts
 * (payload building, URL/headers, response parsing) are handled by callbacks.
 */
struct ChatCompletionRequestHelper {
    /**
     * @brief Execute a chat completion request with common lifecycle management.
     * 
     * @param default_params Provider-specific default parameters
     * @param params Request parameters (will be merged with defaults)
     * @param buildPayload Callback to build provider-specific payload (model, messages, etc.)
     * @param buildUrl Callback to build provider-specific URL
     * @param buildHeaders Callback to build provider-specific headers
     * @param parseResponse Callback to parse provider-specific response format
     * @param exponential_retry Whether to use exponential backoff (default: true)
     * @return APIResponse with parsed content or error details
     */
    template <typename PayloadBuilder, typename UrlBuilder, typename HeaderBuilder, typename ResponseParser>
    static APIResponse execute(
        const nlohmann::json& default_params,
        const nlohmann::json& params,
        PayloadBuilder&& buildPayload,
        UrlBuilder&& buildUrl,
        HeaderBuilder&& buildHeaders,
        ResponseParser&& parseResponse,
        bool exponential_retry = true,
        const IConfigManager* cfg = nullptr) {
        
        APIResponse response;
        response.success = false;
        response.error_code = APIResponse::APIError::Unknown;
        
        try {
            RetrySettings rs = computeRetrySettings(params, cfg, exponential_retry);
            
            // Merge default params with provided params
            nlohmann::json request_params = default_params;
            request_params.update(params);
            
            // Build provider-specific payload once and cache serialized body for retries
            nlohmann::json payload = buildPayload(request_params);
            const std::string serialized_body = payload.dump();
            
            // Get timeout from params or use config default
            int timeout_seconds = 0;
            if (params.contains(std::string(::LLMEngine::Constants::JsonKeys::TIMEOUT_SECONDS)) &&
                params.at(std::string(::LLMEngine::Constants::JsonKeys::TIMEOUT_SECONDS)).is_number_integer()) {
                timeout_seconds = params.at(std::string(::LLMEngine::Constants::JsonKeys::TIMEOUT_SECONDS)).get<int>();
            } else {
                timeout_seconds = cfg ? cfg->getTimeoutSeconds() : APIConfigManager::getInstance().getTimeoutSeconds();
            }
            // Clamp to a safe range [1, 600] seconds
            if (timeout_seconds < 1) timeout_seconds = 1;
            if (timeout_seconds > 600) timeout_seconds = 600;
            
            // Build provider-specific URL and headers
            const std::string url = buildUrl();
            const std::map<std::string, std::string> headers = buildHeaders();
            
            // Log request if enabled
            maybeLogRequest("POST", url, headers);
            
            // Execute request with retries
            cpr::Response cpr_response = sendWithRetries(rs, [&](){
                return cpr::Post(
                    cpr::Url{url},
                    cpr::Header{headers.begin(), headers.end()},
                    cpr::Body{serialized_body},
                    cpr::Timeout{timeout_seconds * MILLISECONDS_PER_SECOND}
                );
            });
            
            response.status_code = static_cast<int>(cpr_response.status_code);
            
            // Parse response using provider-specific parser
            if (response.status_code >= 200 && response.status_code < 300) {
                // Only parse JSON for successful responses
                try {
                    response.raw_response = nlohmann::json::parse(cpr_response.text);
                    parseResponse(response, cpr_response.text);
                } catch (const nlohmann::json::parse_error& e) {
                    response.error_message = "JSON parse error in successful response: " + std::string(e.what());
                    response.error_code = APIResponse::APIError::InvalidResponse;
                }
            } else {
                // For error responses, attempt to parse JSON but don't fail if it's not JSON
                response.error_message = "HTTP " + std::to_string(cpr_response.status_code) + ": " + cpr_response.text;
                if (cpr_response.status_code == HTTP_STATUS_UNAUTHORIZED || cpr_response.status_code == HTTP_STATUS_FORBIDDEN) {
                    response.error_code = APIResponse::APIError::Auth;
                } else if (cpr_response.status_code == HTTP_STATUS_TOO_MANY_REQUESTS) {
                    response.error_code = APIResponse::APIError::RateLimited;
                } else {
                    response.error_code = APIResponse::APIError::Server;
                }
                
                // Attempt to parse error response JSON if available
                if (!cpr_response.text.empty()) {
                    try {
                        response.raw_response = nlohmann::json::parse(cpr_response.text);
                    } catch (const nlohmann::json::parse_error&) {  // NOLINT(bugprone-empty-catch)
                        // Non-JSON error response is acceptable
                    }
                }
            }
            
        } catch (const nlohmann::json::parse_error& e) {
            response.error_message = "JSON parse error: " + std::string(e.what());
            response.error_code = APIResponse::APIError::InvalidResponse;
        } catch (const std::exception& e) {
            response.error_message = "Exception: " + std::string(e.what());
            response.error_code = APIResponse::APIError::Network;
        }
        
        return response;
    }
};

} // namespace LLMEngineAPI

