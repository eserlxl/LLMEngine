// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include <cstdint>

namespace LLMEngine {

/**
 * @brief HTTP status code constants for type safety and clarity.
 *
 * These constants provide a more readable and maintainable way to work
 * with HTTP status codes throughout the codebase.
 */
namespace HttpStatus {
// Success codes (2xx)
constexpr int OK = 200;
constexpr int CREATED = 201;
constexpr int NO_CONTENT = 204;
constexpr int SUCCESS_MIN = 200;
constexpr int SUCCESS_MAX = 299;

// Client error codes (4xx)
constexpr int BAD_REQUEST = 400;
constexpr int UNAUTHORIZED = 401;
constexpr int FORBIDDEN = 403;
constexpr int NOT_FOUND = 404;
constexpr int TOO_MANY_REQUESTS = 429;
constexpr int CLIENT_ERROR_MIN = 400;
constexpr int CLIENT_ERROR_MAX = 499;

// Server error codes (5xx)
constexpr int INTERNAL_SERVER_ERROR = 500;
constexpr int BAD_GATEWAY = 502;
constexpr int SERVICE_UNAVAILABLE = 503;
constexpr int SERVER_ERROR_MIN = 500;
constexpr int SERVER_ERROR_MAX = 599;

/**
 * @brief Check if a status code indicates success (2xx).
 */
constexpr bool isSuccess(int status_code) {
    return status_code >= SUCCESS_MIN && status_code < SUCCESS_MAX + 1;
}

/**
 * @brief Check if a status code indicates a client error (4xx).
 */
constexpr bool isClientError(int status_code) {
    return status_code >= CLIENT_ERROR_MIN && status_code < CLIENT_ERROR_MAX + 1;
}

/**
 * @brief Check if a status code indicates a server error (5xx).
 */
constexpr bool isServerError(int status_code) {
    return status_code >= SERVER_ERROR_MIN && status_code < SERVER_ERROR_MAX + 1;
}

/**
 * @brief Check if an error status code should be retried.
 *
 * Returns true for:
 * - 429 (Too Many Requests) - rate limiting, should retry
 * - 5xx (Server errors) - transient errors, should retry
 *
 * Returns false for:
 * - 4xx (except 429) - client errors, should not retry
 */
constexpr bool isRetriable(int status_code) {
    return status_code == TOO_MANY_REQUESTS || isServerError(status_code);
}
} // namespace HttpStatus

} // namespace LLMEngine
