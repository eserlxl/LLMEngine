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
constexpr int ok = 200;
constexpr int created = 201;
constexpr int noContent = 204;
constexpr int successMin = 200;
constexpr int successMax = 299;

// Client error codes (4xx)
constexpr int badRequest = 400;
constexpr int unauthorized = 401;
constexpr int forbidden = 403;
constexpr int notFound = 404;
constexpr int tooManyRequests = 429;
constexpr int clientErrorMin = 400;
constexpr int clientErrorMax = 499;

// Server error codes (5xx)
constexpr int internalServerError = 500;
constexpr int badGateway = 502;
constexpr int serviceUnavailable = 503;
constexpr int serverErrorMin = 500;
constexpr int serverErrorMax = 599;

/**
 * @brief Check if a status code indicates success (2xx).
 */
constexpr bool isSuccess(int status_code) {
    return status_code >= successMin && status_code < successMax + 1;
}

/**
 * @brief Check if a status code indicates a client error (4xx).
 */
constexpr bool isClientError(int status_code) {
    return status_code >= clientErrorMin && status_code < clientErrorMax + 1;
}

/**
 * @brief Check if a status code indicates a server error (5xx).
 */
constexpr bool isServerError(int status_code) {
    return status_code >= serverErrorMin && status_code < serverErrorMax + 1;
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
    return status_code == tooManyRequests || isServerError(status_code);
}
} // namespace HttpStatus

} // namespace LLMEngine
