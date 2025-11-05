// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include "LLMEngine/APIClient.hpp"
#include "LLMEngine/RequestLogger.hpp"
#include "LLMEngine/HttpStatus.hpp"
#include "Backoff.hpp"
#include "LLMEngine/Constants.hpp"
#include <cpr/cpr.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <map>
#include <cstring>

namespace LLMEngineAPI {

// Internal constants (not exposed in public header)
namespace {
    constexpr int MILLISECONDS_PER_SECOND = 1000;
}

// Shared helpers for provider clients
namespace {
    struct RetrySettings {
        int max_attempts;
        int base_delay_ms;
        int max_delay_ms;
        uint64_t jitter_seed;
        bool exponential;
    };

    inline RetrySettings computeRetrySettings(const nlohmann::json& params, const IConfigManager* cfg, bool exponential_default = true) {
        RetrySettings rs{};
        if (cfg) {
            rs.max_attempts = std::max(1, cfg->getRetryAttempts());
            rs.base_delay_ms = std::max(0, cfg->getRetryDelayMs());
        } else {
            rs.max_attempts = std::max(1, APIConfigManager::getInstance().getRetryAttempts());
            rs.base_delay_ms = std::max(0, APIConfigManager::getInstance().getRetryDelayMs());
        }
        rs.max_delay_ms = ::LLMEngine::Constants::DefaultValues::MAX_BACKOFF_DELAY_MS;
        rs.jitter_seed = 0;
        rs.exponential = exponential_default;
        if (params.contains("retry_attempts")) rs.max_attempts = std::max(1, params["retry_attempts"].get<int>());
        if (params.contains("retry_base_delay_ms")) rs.base_delay_ms = std::max(0, params["retry_base_delay_ms"].get<int>());
        if (params.contains("retry_max_delay_ms")) rs.max_delay_ms = std::max(0, params["retry_max_delay_ms"].get<int>());
        if (params.contains("jitter_seed")) rs.jitter_seed = params["jitter_seed"].get<uint64_t>();
        if (params.contains("retry_exponential")) rs.exponential = params["retry_exponential"].get<bool>();
        return rs;
    }

    template <typename RequestFunc>
    cpr::Response sendWithRetries(const RetrySettings& rs, RequestFunc&& doRequest) {
        // Optional trace logging for backoff schedule (only when explicitly enabled)
        const bool log_backoff = std::getenv("LLMENGINE_LOG_BACKOFF") != nullptr;
        
        cpr::Response resp;
        ::LLMEngine::BackoffConfig bcfg{rs.base_delay_ms, rs.max_delay_ms};
        std::unique_ptr<std::mt19937_64> rng;
        if (rs.jitter_seed != 0 && rs.exponential) {
            rng = std::make_unique<std::mt19937_64>(rs.jitter_seed);
        }
        for (int attempt = 1; attempt <= rs.max_attempts; ++attempt) {
            resp = doRequest();
            const int code = static_cast<int>(resp.status_code);
            const bool is_success = ::LLMEngine::HttpStatus::isSuccess(code);
            if (is_success) {
                if (log_backoff && attempt > 1) {
                    std::cerr << "[BACKOFF] Request succeeded after " << attempt << " attempt(s)\n";
                }
                break;
            }
            const bool is_non_retriable = ::LLMEngine::HttpStatus::isClientError(code) && !::LLMEngine::HttpStatus::isRetriable(code);
            if (attempt < rs.max_attempts && !is_non_retriable) {
                int delay = 0;
                if (rs.exponential) {
                    const uint64_t cap = ::LLMEngine::computeBackoffCapMs(bcfg, attempt);
                    if (rng) {
                        delay = ::LLMEngine::jitterDelayMs(*rng, cap);
                    } else {
                        delay = static_cast<int>(cap);
                    }
                } else {
                    delay = rs.base_delay_ms * attempt;
                }
                
                if (log_backoff) {
                    std::cerr << "[BACKOFF] Attempt " << attempt << " failed (HTTP " << code 
                              << "), retrying after " << delay << "ms";
                    if (rs.exponential) {
                        std::cerr << " (exponential backoff, cap=" << (rng ? "jittered" : std::to_string(delay)) << "ms)";
                    } else {
                        std::cerr << " (linear backoff)";
                    }
                    std::cerr << "\n";
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            } else {
                if (log_backoff) {
                    std::cerr << "[BACKOFF] Request failed after " << attempt << " attempt(s)";
                    if (is_non_retriable) {
                        std::cerr << " (non-retriable error: HTTP " << code << ")";
                    } else {
                        std::cerr << " (max attempts reached)";
                    }
                    std::cerr << "\n";
                }
                break;
            }
        }
        return resp;
    }

    inline void maybeLogRequest(std::string_view method, std::string_view url,
                                const std::map<std::string, std::string>& headers) {
        if (std::getenv("LLMENGINE_LOG_REQUESTS") != nullptr) {
            std::cerr << LLMEngine::RequestLogger::formatRequest(method, url, headers);
        }
    }

    inline void maybeLogRequestWithBody(std::string_view method, std::string_view url,
                                        const std::map<std::string, std::string>& headers,
                                        std::string_view body) {
        if (std::getenv("LLMENGINE_LOG_REQUESTS") == nullptr) return;
        std::cerr << LLMEngine::RequestLogger::formatRequest(method, url, headers);
        // Optional capped, redacted body logging when explicitly enabled
        // SECURITY: Body logging is opt-in only and strictly limited to prevent credential leakage
        if (std::getenv("LLMENGINE_LOG_REQUESTS_BODY") != nullptr) {
            constexpr size_t kMaxBytes = 512;  // Strict cap to prevent excessive logging
            constexpr size_t kMinSizeForWarning = 10000;  // Warn if body is very large
            const size_t n = std::min(kMaxBytes, body.size());
            
            // Warn if body exceeds safe size (potential for credential leakage)
            if (body.size() > kMinSizeForWarning) {
                std::cerr << "[WARNING] Request body size (" << body.size() 
                          << " bytes) exceeds safe logging threshold. Only first " 
                          << kMaxBytes << " bytes will be logged.\n";
            }
            
            const std::string prefix(body.substr(0, n));
            const std::string redacted = LLMEngine::RequestLogger::redactText(prefix);
            std::cerr << "Body (first " << n << " bytes, redacted";
            if (body.size() > n) {
                std::cerr << ", truncated from " << body.size() << " bytes";
            }
            std::cerr << "):\n" << redacted << "\n";
        }
    }
}

} // namespace LLMEngineAPI

