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

        // 1. Load history if present
        if (input.contains("messages") && input["messages"].is_array()) {
            messages = input["messages"];
        } else {
            // 2. Legacy: Add system message if input contains system prompt and no history
            if (input.contains(std::string(::LLMEngine::Constants::JsonKeys::SYSTEM_PROMPT))) {
                const auto& sp =
                    input.at(std::string(::LLMEngine::Constants::JsonKeys::SYSTEM_PROMPT));
                if (sp.is_string()) {
                    messages.push_back({{"role", "system"}, {"content", sp.get<std::string>()}});
                } else if (sp.is_number() || sp.is_boolean()) {
                    messages.push_back({{"role", "system"}, {"content", sp.dump()}});
                }
            }
        }

        // 3. Construct user message content (Text or Multi-modal)
        nlohmann::json user_content;

        // check for images in input
        if (input.contains("images") && input["images"].is_array() && !input["images"].empty()) {
            // Multi-modal format
            user_content = nlohmann::json::array();
            // Add text part
            user_content.push_back({{"type", "text"}, {"text", prompt}});

            // Add image parts
            for (const auto& img : input["images"]) {
                if (img.is_string()) {
                    user_content.push_back(
                        {{"type", "image_url"}, {"image_url", {{"url", img.get<std::string>()}}}});
                }
            }
        } else {
            // Simple text format
            user_content = prompt;
        }

        // 4. Append user message if prompt or images are present
        if (!prompt.empty() || (input.contains("images") && input["images"].is_array() && !input["images"].empty())) {
            messages.push_back({{"role", "user"}, {"content", user_content}});
        }

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
                               const ::LLMEngine::RequestOptions& options = {},
                               bool exponential_retry = true,
                               const IConfigManager* cfg = nullptr) {

        APIResponse response;
        response.success = false;
        response.errorCode = LLMEngine::LLMEngineErrorCode::Unknown;

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
            
            // Inject options overrides into request params if present
            // We cast away constness locally or (better) ensure we are working on a copy if we need to modify.
            // If request_params_ptr points to default_params (const), we must copy if we want to inject.
            // But efficient way: merge into a new json object only if options are present?
            // Or just always use a mutable copy if options are present.
            
            nlohmann::json final_params_storage;
            const nlohmann::json* effective_params_ptr = request_params_ptr;

            if (options.generation.user.has_value() || 
                options.generation.logprobs.has_value() ||
                options.generation.top_k.has_value() ||
                options.generation.min_p.has_value() ||
                options.generation.seed.has_value() ||
                options.generation.parallel_tool_calls.has_value() ||
                options.generation.service_tier.has_value() ||
                options.stream_options.has_value()) {
                
                final_params_storage = *request_params_ptr;
                if (options.generation.user.has_value()) {
                    final_params_storage[std::string(::LLMEngine::Constants::JsonKeys::USER)] = *options.generation.user;
                }
                if (options.generation.logprobs.has_value()) {
                    final_params_storage[std::string(::LLMEngine::Constants::JsonKeys::LOGPROBS)] = *options.generation.logprobs;
                    if (options.generation.top_logprobs.has_value()) {
                        final_params_storage[std::string(::LLMEngine::Constants::JsonKeys::TOP_LOGPROBS)] = *options.generation.top_logprobs;
                    }
                }
                if (options.generation.top_k.has_value()) {
                    final_params_storage[std::string(::LLMEngine::Constants::JsonKeys::TOP_K)] = *options.generation.top_k;
                }
                if (options.generation.min_p.has_value()) {
                    final_params_storage[std::string(::LLMEngine::Constants::JsonKeys::MIN_P)] = *options.generation.min_p;
                }
                if (options.generation.seed.has_value()) {
                    final_params_storage["seed"] = *options.generation.seed;
                }
                if (options.generation.parallel_tool_calls.has_value()) {
                    final_params_storage["parallel_tool_calls"] = *options.generation.parallel_tool_calls;
                }
                if (options.generation.service_tier.has_value()) {
                    final_params_storage["service_tier"] = *options.generation.service_tier;
                }
                if (options.stream_options.has_value()) {
                    final_params_storage["stream_options"] = {{"include_usage", options.stream_options->include_usage}};
                }
                effective_params_ptr = &final_params_storage;
            }

            const nlohmann::json& request_params = *effective_params_ptr;

            // Build provider-specific payload once and cache serialized body for retries
            nlohmann::json payload = buildPayload(request_params);
            const std::string serialized_body = payload.dump();

            // Get timeout from params or use config default
            // Get timeout from options (highest priority), then params, then config
            int timeout_seconds = 0;

            if (options.timeout_ms.has_value()) {
                // round up to nearest second
                timeout_seconds = (*options.timeout_ms + (::LLMEngine::Constants::DefaultValues::MILLISECONDS_PER_SECOND - 1)) / 
                                  ::LLMEngine::Constants::DefaultValues::MILLISECONDS_PER_SECOND;
            } else if (params.contains(
                           std::string(::LLMEngine::Constants::JsonKeys::TIMEOUT_SECONDS))
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
            if (connect_timeout_ms > ::LLMEngine::Constants::DefaultValues::MAX_CONNECT_TIMEOUT_MS)
                connect_timeout_ms = ::LLMEngine::Constants::DefaultValues::MAX_CONNECT_TIMEOUT_MS;

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
            std::map<std::string, std::string> headers = buildHeaders();
            // Merge extra headers from options
            for (const auto& [key, value] : options.extra_headers) {
                headers[key] = value;
            }
            // Pre-allocate cpr::Header once to avoid repeated map iteration in retry loop
            const cpr::Header cpr_headers{headers.begin(), headers.end()};

            // Log request if enabled (with capped, redacted body when explicitly requested)
            maybeLogRequestWithBody("POST", url, headers, serialized_body);

            // Execute request with retries
            cpr::Response cpr_response = sendWithRetries(
                rs,
                [&]() {
                    if (connect_timeout_ms > 0) {
                        return cpr::Post(cpr::Url{url},
                                         cpr_headers,
                                         cpr::Body{serialized_body},
                                         cpr::Timeout{timeout_seconds * MILLISECONDS_PER_SECOND},
                                         cpr::VerifySsl{verify_ssl},
                                         cpr::ConnectTimeout{connect_timeout_ms},
                                         cpr::ProgressCallback([&](size_t, size_t, size_t, size_t, intptr_t) -> bool {
                                             if (options.cancellation_token && options.cancellation_token->isCancelled()) {
                                                 return false; // Cancel request
                                             }
                                             return true;
                                         }));
                    } else {
                        return cpr::Post(cpr::Url{url},
                                         cpr_headers,
                                         cpr::Body{serialized_body},
                                         cpr::Timeout{timeout_seconds * MILLISECONDS_PER_SECOND},
                                         cpr::VerifySsl{verify_ssl},
                                         cpr::ProgressCallback([&](size_t, size_t, size_t, size_t, intptr_t) -> bool {
                                             if (options.cancellation_token && options.cancellation_token->isCancelled()) {
                                                 return false; // Cancel request
                                             }
                                             return true;
                                         }));
                    }
                },
                options);

            response.statusCode = static_cast<int>(cpr_response.status_code);

            // Parse response using provider-specific parser
            if (::LLMEngine::HttpStatus::isSuccess(response.statusCode)) {
                // Only parse JSON for successful responses
                try {
                    response.rawResponse = nlohmann::json::parse(cpr_response.text);
                    parseResponse(response, cpr_response.text);
                } catch (const nlohmann::json::parse_error& e) {
                    response.errorMessage =
                        "JSON parse error in successful response: " + std::string(e.what());
                    response.errorCode = LLMEngine::LLMEngineErrorCode::InvalidResponse;
                }
            } else {
                // For error responses, attempt to parse JSON but don't fail if it's not JSON
                // Build enhanced error message with context
                std::string error_msg = "HTTP " + std::to_string(cpr_response.status_code);
                if (!cpr_response.text.empty()) {
                    // Try to extract structured error message from JSON response
                    try {
                        auto error_json = nlohmann::json::parse(cpr_response.text);
                        response.rawResponse = error_json;

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
                        response.rawResponse = nlohmann::json::object();
                    }
                } else {
                    error_msg += ": Empty response body";
                }

                response.errorMessage = error_msg;

                // Classify error based on HTTP status code
                if (cpr_response.status_code == ::LLMEngine::HttpStatus::UNAUTHORIZED
                    || cpr_response.status_code == ::LLMEngine::HttpStatus::FORBIDDEN) {
                    response.errorCode = LLMEngine::LLMEngineErrorCode::Auth;
                } else if (cpr_response.status_code == ::LLMEngine::HttpStatus::TOO_MANY_REQUESTS) {
                    response.errorCode = LLMEngine::LLMEngineErrorCode::RateLimited;
                } else if (::LLMEngine::HttpStatus::isServerError(
                               static_cast<int>(cpr_response.status_code))) {
                    response.errorCode = LLMEngine::LLMEngineErrorCode::Server;
                } else if (::LLMEngine::HttpStatus::isClientError(
                               static_cast<int>(cpr_response.status_code))) {
                    response.errorCode = LLMEngine::LLMEngineErrorCode::Client;
                } else {
                    response.errorCode = LLMEngine::LLMEngineErrorCode::Unknown;
                }
            }

        } catch (const nlohmann::json::parse_error& e) {
            response.errorMessage = "JSON parse error at position " + std::to_string(e.byte) + ": "
                                     + std::string(e.what());
            response.errorCode = LLMEngine::LLMEngineErrorCode::InvalidResponse;
        } catch (const std::exception& e) {
            response.errorMessage = "Network error: " + std::string(e.what());
            response.errorCode = LLMEngine::LLMEngineErrorCode::Network;
        } catch (...) {
            response.errorMessage = "Unknown error occurred during request execution";
            response.errorCode = LLMEngine::LLMEngineErrorCode::Unknown;
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
                              const ::LLMEngine::RequestOptions& options = {},
                              bool exponential_retry = true,
                              const IConfigManager* cfg = nullptr) {
        (void)exponential_retry;

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
        std::map<std::string, std::string> headers = buildHeaders();
        // Merge extra headers from options
        for (const auto& [key, value] : options.extra_headers) {
            headers[key] = value;
        }
        const cpr::Header cpr_headers{headers.begin(), headers.end()};

        // Timeout settings
        // Timeout settings
        int timeout_seconds = 0;
        if (options.timeout_ms.has_value()) {
            timeout_seconds = (*options.timeout_ms + (::LLMEngine::Constants::DefaultValues::MILLISECONDS_PER_SECOND - 1)) / 
                              ::LLMEngine::Constants::DefaultValues::MILLISECONDS_PER_SECOND;
        } else {
            timeout_seconds = cfg ? cfg->getTimeoutSeconds()
                                  : APIConfigManager::getInstance().getTimeoutSeconds();
        }

        bool verify_ssl = true;
        if (params.contains("verify_ssl") && params.at("verify_ssl").is_boolean()) {
            verify_ssl = params.at("verify_ssl").get<bool>();
        }

        // Execute request with WriteCallback
        cpr::Response response = cpr::Post(cpr::Url{url},
                                           cpr_headers,
                                           cpr::Body{serialized_body},
                                           cpr::Timeout{timeout_seconds * ::LLMEngine::Constants::DefaultValues::MILLISECONDS_PER_SECOND},
                                           cpr::VerifySsl{verify_ssl},
                                           cpr::WriteCallback([&](std::string_view data, intptr_t) {
                                               if (options.cancellation_token && options.cancellation_token->isCancelled()) {
                                                   return false; // Cancel request
                                               }
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
