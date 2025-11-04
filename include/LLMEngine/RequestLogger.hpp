// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include <string>
#include <string_view>
#include <map>
#include <vector>
#include "LLMEngineExport.hpp"

/**
 * @brief Centralized request logging utility with automatic redaction of sensitive data.
 * 
 * This utility provides methods to safely log HTTP requests, URLs, and headers
 * by automatically redacting API keys, tokens, and other sensitive information
 * to prevent accidental credential leakage in logs.
 */
class LLMENGINE_EXPORT RequestLogger {
public:
    /**
     * @brief Redact sensitive query parameters from a URL.
     * 
     * Removes or masks query parameters that typically contain sensitive data
     * (e.g., "key", "api_key", "token", "access_token").
     * 
     * @param url Original URL (may contain query parameters)
     * @return URL with sensitive query parameters redacted
     */
    static std::string redactUrl(std::string_view url);

    /**
     * @brief Redact sensitive headers from a header map.
     * 
     * Replaces values of sensitive header names (e.g., "authorization", "x-api-key")
     * with "<REDACTED>" to prevent credential leakage.
     * 
     * @param headers Map of header names to values
     * @return Map with sensitive header values redacted
     */
    static std::map<std::string, std::string> redactHeaders(const std::map<std::string, std::string>& headers);

    /**
     * @brief Create a safe log message for an HTTP request.
     * 
     * Formats a request for logging with automatic redaction of URLs and headers.
     * 
     * @param method HTTP method (e.g., "POST", "GET")
     * @param url Request URL
     * @param headers Request headers
     * @return Safe log message string with redactions applied
     */
    static std::string formatRequest(std::string_view method, 
                                     std::string_view url,
                                     const std::map<std::string, std::string>& headers);

private:
    // List of sensitive query parameter names to redact
    static const std::vector<std::string>& getSensitiveQueryParams();
    
    // List of sensitive header names to redact
    static const std::vector<std::string>& getSensitiveHeaderNames();
    
    // Check if a header name is sensitive (case-insensitive)
    static bool isSensitiveHeader(std::string_view header_name);
};


