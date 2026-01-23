// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include "APIClientCommon.hpp"
#include "LLMEngine/APIClient.hpp"
#include "LLMEngine/Constants.hpp"
#include "LLMEngine/HttpStatus.hpp"

#include <cpr/cpr.h>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

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
        messages.push_back({{"role", "user"}, {"content", prompt}});

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
    template <typename PayloadBuilder,
              typename UrlBuilder,
              typename HeaderBuilder,
              typename ResponseParser>
    static APIResponse execute(const nlohmann::json& default_params,
                               const nlohmann::json& params,
                               PayloadBuilder&& buildPayload,
                               UrlBuilder&& buildUrl,
                               HeaderBuilder&& buildHeaders,
                               ResponseParser&& parseResponse,
                               bool exponential_retry = true,
                               const IConfigManager* cfg = nullptr) {

        APIResponse response;
        response.success = false;
        response.error_code = LLMEngine::LLMEngineErrorCode::Unknown;

        try {
            RetrySettings rs = computeRetrySettings(params, cfg, exponential_retry);

            // Merge default params with provided params
            // Optimize: avoid copy when params is empty by using const reference
            const nlohmann::json* request_params_ptr;
            nlohmann::json request_params_merged;
            if (params.empty() || params.is_null()) {
                // No overrides: use default_params directly without copying
                request_params_ptr = &default_params;
            } else {
                // Overrides present: merge into a copy
                request_params_merged = default_params;
                request_params_merged.update(params);
                request_params_ptr = &request_params_merged;
            }
            const nlohmann::json& request_params = *request_params_ptr;

            // Build provider-specific payload once and cache serialized body for retries
            nlohmann::json payload = buildPayload(request_params);
            const std::string serialized_body = payload.dump();

            // Get timeout from params or use config default
            int timeout_seconds = 0;
            if (params.contains(std::string(::LLMEngine::Constants::JsonKeys::TIMEOUT_SECONDS))
                && params.at(std::string(::LLMEngine::Constants::JsonKeys::TIMEOUT_SECONDS))
                       .is_number_integer()) {
                timeout_seconds =
                    params.at(std::string(::LLMEngine::Constants::JsonKeys::TIMEOUT_SECONDS))
                        .get<int>();
            } else {
                timeout_seconds = cfg ? cfg->getTimeoutSeconds()
                                      : APIConfigManager::getInstance().getTimeoutSeconds();
            }
            // Clamp to a safe range [1, MAX_TIMEOUT_SECONDS] seconds
            timeout_seconds = std::max(timeout_seconds, 1);
            timeout_seconds = std::min(timeout_seconds,
                                       ::LLMEngine::Constants::DefaultValues::MAX_TIMEOUT_SECONDS);

            // Optional connect timeout override in milliseconds
            int connect_timeout_ms = 0;
            if (params.contains("connect_timeout_ms")
                && params.at("connect_timeout_ms").is_number_integer()) {
                connect_timeout_ms = params.at("connect_timeout_ms").get<int>();
            }
            if (connect_timeout_ms < 0)
                connect_timeout_ms = 0;
            if (connect_timeout_ms > 120000)
                connect_timeout_ms = 120000; // cap at 120s

            // SSL verification toggle (default: true)
            // SECURITY: Log prominently when verification is disabled to prevent accidental use in
            // production
            bool verify_ssl = true;
            if (params.contains("verify_ssl") && params.at("verify_ssl").is_boolean()) {
                verify_ssl = params.at("verify_ssl").get<bool>();
                if (!verify_ssl) {
                    // Log prominently when TLS verification is disabled
                    // This helps prevent accidental use in production environments
                    std::cerr << "[LLMEngine SECURITY WARNING] TLS verification is DISABLED for "
                                 "this request. "
                              << "This should only be used in development/testing environments. "
                              << "Disabling TLS verification in production exposes the application "
                                 "to man-in-the-middle attacks.\n";
                }
            }

            // Build provider-specific URL and headers once before retries
            // Headers are typically cached by the client, so we capture the reference/const
            // reference to avoid redundant allocations in tight retry loops
            const std::string url = buildUrl();
            const std::map<std::string, std::string> headers = buildHeaders();
            // Pre-allocate cpr::Header once to avoid repeated map iteration in retry loop
            const cpr::Header cpr_headers{headers.begin(), headers.end()};

            // Log request if enabled (with capped, redacted body when explicitly requested)
            maybeLogRequestWithBody("POST", url, headers, serialized_body);

            // Execute request with retries
            cpr::Response cpr_response = sendWithRetries(rs, [&]() {
                if (connect_timeout_ms > 0) {
                    return cpr::Post(cpr::Url{url},
                                     cpr_headers,
                                     cpr::Body{serialized_body},
                                     cpr::Timeout{timeout_seconds * MILLISECONDS_PER_SECOND},
                                     cpr::VerifySsl{verify_ssl},
                                     cpr::ConnectTimeout{connect_timeout_ms});
                } else {
                    return cpr::Post(cpr::Url{url},
                                     cpr_headers,
                                     cpr::Body{serialized_body},
                                     cpr::Timeout{timeout_seconds * MILLISECONDS_PER_SECOND},
                                     cpr::VerifySsl{verify_ssl});
                }
            });

            response.status_code = static_cast<int>(cpr_response.status_code);

            // Parse response using provider-specific parser
            if (::LLMEngine::HttpStatus::isSuccess(response.status_code)) {
                // Only parse JSON for successful responses
                try {
                    response.raw_response = nlohmann::json::parse(cpr_response.text);
                    parseResponse(response, cpr_response.text);
                } catch (const nlohmann::json::parse_error& e) {
                    response.error_message =
                        "JSON parse error in successful response: " + std::string(e.what());
                    response.error_code = LLMEngine::LLMEngineErrorCode::InvalidResponse;
                }
            } else {
                // For error responses, attempt to parse JSON but don't fail if it's not JSON
                // Build enhanced error message with context
                std::string error_msg = "HTTP " + std::to_string(cpr_response.status_code);
                if (!cpr_response.text.empty()) {
                    // Try to extract structured error message from JSON response
                    try {
                        auto error_json = nlohmann::json::parse(cpr_response.text);
                        response.raw_response = error_json;

                        // Extract error message from common JSON error response formats
                        if (error_json.contains("error")) {
                            const auto& error_obj = error_json["error"];
                            if (error_obj.is_object() && error_obj.contains("message")) {
                                error_msg += ": " + error_obj["message"].get<std::string>();
                            } else if (error_obj.is_string()) {
                                error_msg += ": " + error_obj.get<std::string>();
                            } else {
                                error_msg += ": " + cpr_response.text;
                            }
                        } else if (error_json.contains("message")) {
                            error_msg += ": " + error_json["message"].get<std::string>();
                        } else {
                            error_msg += ": " + cpr_response.text;
                        }
                    } catch (const nlohmann::json::parse_error& e) { // NOLINT(bugprone-empty-catch)
                        // Non-JSON error response - use raw text
                        // This is expected behavior: some error responses may not be valid JSON.
                        // We intentionally swallow the parse error and use the raw response text
                        // instead. Logging is not available in this context, but the error is
                        // handled by including the raw response text in the error message.
                        error_msg += ": " + cpr_response.text;
                        response.raw_response = nlohmann::json::object();
                    }
                } else {
                    error_msg += ": Empty response body";
                }

                response.error_message = error_msg;

                // Classify error based on HTTP status code
                if (cpr_response.status_code == ::LLMEngine::HttpStatus::UNAUTHORIZED
                    || cpr_response.status_code == ::LLMEngine::HttpStatus::FORBIDDEN) {
                    response.error_code = LLMEngine::LLMEngineErrorCode::Auth;
                } else if (cpr_response.status_code == ::LLMEngine::HttpStatus::TOO_MANY_REQUESTS) {
                    response.error_code = LLMEngine::LLMEngineErrorCode::RateLimited;
                } else if (::LLMEngine::HttpStatus::isServerError(
                               static_cast<int>(cpr_response.status_code))) {
                    response.error_code = LLMEngine::LLMEngineErrorCode::Server;
                } else if (::LLMEngine::HttpStatus::isClientError(
                               static_cast<int>(cpr_response.status_code))) {
                    response.error_code = LLMEngine::LLMEngineErrorCode::Client;
                } else {
                    response.error_code = LLMEngine::LLMEngineErrorCode::Unknown;
                }
            }

        } catch (const nlohmann::json::parse_error& e) {
            response.error_message = "JSON parse error at position " + std::to_string(e.byte) + ": "
                                     + std::string(e.what());
            response.error_code = LLMEngine::LLMEngineErrorCode::InvalidResponse;
        } catch (const std::exception& e) {
            response.error_message = "Network error: " + std::string(e.what());
            response.error_code = LLMEngine::LLMEngineErrorCode::Network;
        } catch (...) {
            response.error_message = "Unknown error occurred during request execution";
            response.error_code = LLMEngine::LLMEngineErrorCode::Unknown;
        }

        return response;
    }

    /**
     * @brief Execute a streaming chat completion request.
     *
     * @tparam StreamProcessor Callable handling data chunks: void(std::string_view)
     */
    template <typename PayloadBuilder,
              typename UrlBuilder,
              typename HeaderBuilder,
              typename StreamProcessor>
    static void executeStream(const nlohmann::json& default_params,
                              const nlohmann::json& params,
                              PayloadBuilder&& buildPayload,
                              UrlBuilder&& buildUrl,
                              HeaderBuilder&& buildHeaders,
                              StreamProcessor&& processChunk,
                              const IConfigManager* cfg = nullptr) {

        // Merge params (simplified vs execute, no retry settings needed generally for stream connection yet)
        nlohmann::json request_params_merged;
        const nlohmann::json* request_params_ptr;
        if (params.empty() || params.is_null()) {
            request_params_ptr = &default_params;
        } else {
            request_params_merged = default_params;
            request_params_merged.update(params);
            request_params_ptr = &request_params_merged;
        }
        const nlohmann::json& request_params = *request_params_ptr;

        nlohmann::json payload = buildPayload(request_params);
        // Force stream=true if not already set (caller usually handles this, but safety check?)
        // Better to rely on caller.

        const std::string serialized_body = payload.dump();
        const std::string url = buildUrl();
        const std::map<std::string, std::string> headers = buildHeaders();
        const cpr::Header cpr_headers{headers.begin(), headers.end()};

        // Timeout settings
        int timeout_seconds =
            cfg ? cfg->getTimeoutSeconds() : APIConfigManager::getInstance().getTimeoutSeconds();
        // For streaming, timeout usually applies to connect or idle?
        // cpr::Timeout is transfer timeout.
        // We might want a longer timeout for stream? Or rely on heartbeat.
        // Use standard timeout for now.

        bool verify_ssl = true;
        if (params.contains("verify_ssl") && params.at("verify_ssl").is_boolean()) {
            verify_ssl = params.at("verify_ssl").get<bool>();
        }

        // Execute request with WriteCallback
        cpr::Response response = cpr::Post(cpr::Url{url},
                                           cpr_headers,
                                           cpr::Body{serialized_body},
                                           cpr::Timeout{timeout_seconds * MILLISECONDS_PER_SECOND},
                                           cpr::VerifySsl{verify_ssl},
                                           cpr::WriteCallback([&](std::string_view data, intptr_t) {
                                               processChunk(data);
                                               return true; // continue
                                           }));

        if (response.status_code != 200) {
            // Handle error?
            // Since it's streaming, we might have already processed some chunks?
            // Or if status != 200, typically WriteCallback is called for error body?
            // We should probably check status and throw if failed?
            // But strict error handling for stream is tricky.
            // For now, logging error is enough.
            std::cerr << "Stream request failed with status " << response.status_code << ": "
                      << response.text << std::endl;
            throw std::runtime_error("Stream request failed with status "
                                     + std::to_string(response.status_code));
        }
    }
};

} // namespace LLMEngineAPI
