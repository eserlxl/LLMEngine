// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "LLMEngine/CancellationToken.hpp"
#include <map>
#include <memory>
#include <optional>
#include <string>

namespace LLMEngine {

/**
 * @brief Per-request options for LLM operations.
 */
struct RequestOptions {
    /**
     * @brief Request timeout in milliseconds.
     * Overrides default configuration if set.
     */
    std::optional<int> timeout_ms;

    /**
     * @brief Maximum number of retries for this request.
     * Overrides default configuration if set.
     */
    std::optional<int> max_retries;

    /**
     * @brief Extra HTTP headers to include in the request.
     */
    std::map<std::string, std::string> extra_headers;

    /**
     * @brief Token for cancelling the operation.
     */
    std::shared_ptr<CancellationToken> cancellation_token;

    /**
     * @brief Maximum concurrency for batch operations.
     * Functions like analyzeBatch will limit parallel requests to this number.
     * Default depends on implementation (e.g., usually unbounded or high limit).
     */
    std::optional<size_t> max_concurrency;
};

} // namespace LLMEngine
