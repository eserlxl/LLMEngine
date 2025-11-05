// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include "LLMEngine/APIClient.hpp"
#include "LLMEngine/RequestLogger.hpp"
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
    constexpr int HTTP_STATUS_OK = 200;
    constexpr int HTTP_STATUS_UNAUTHORIZED = 401;
    constexpr int HTTP_STATUS_FORBIDDEN = 403;
    constexpr int HTTP_STATUS_TOO_MANY_REQUESTS = 429;
    constexpr int HTTP_STATUS_SERVER_ERROR_MIN = 500;
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
        cpr::Response resp;
        ::LLMEngine::BackoffConfig bcfg{rs.base_delay_ms, rs.max_delay_ms};
        std::unique_ptr<std::mt19937_64> rng;
        if (rs.jitter_seed != 0 && rs.exponential) {
            rng = std::make_unique<std::mt19937_64>(rs.jitter_seed);
        }
        for (int attempt = 1; attempt <= rs.max_attempts; ++attempt) {
            resp = doRequest();
            const int code = static_cast<int>(resp.status_code);
            const bool is_success = (code >= 200 && code < 300);
            if (is_success) break;
            const bool is_non_retriable = (code >= 400 && code < 500) && (code != HTTP_STATUS_TOO_MANY_REQUESTS);
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
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            } else {
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
        if (std::getenv("LLMENGINE_LOG_REQUESTS_BODY") != nullptr) {
            constexpr size_t kMaxBytes = 512;
            const size_t n = std::min(kMaxBytes, body.size());
            const std::string prefix(body.substr(0, n));
            const std::string redacted = LLMEngine::RequestLogger::redactText(prefix);
            std::cerr << "Body (first " << n << " bytes, redacted):\n" << redacted << "\n";
        }
    }
}

} // namespace LLMEngineAPI

