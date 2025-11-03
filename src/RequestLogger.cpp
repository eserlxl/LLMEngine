// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "RequestLogger.hpp"
#include <algorithm>
#include <sstream>
#include <cctype>

namespace {
    const std::vector<std::string>& getSensitiveQueryParamsImpl() {
        static const std::vector<std::string> params = {
            "key", "api_key", "apikey", "token", "access_token", "accesstoken",
            "secret", "password", "passwd", "pwd", "credential", "auth",
            "authorization", "x-api-key", "xapikey", "apikey"
        };
        return params;
    }

    const std::vector<std::string>& getSensitiveHeaderNamesImpl() {
        static const std::vector<std::string> headers = {
            "authorization", "x-api-key", "xapikey", "apikey", "x-goog-api-key",
            "x-auth-token", "api-key", "access-token", "secret", "password",
            "cookie", "set-cookie"
        };
        return headers;
    }

    std::string toLower(std::string_view str) {
        std::string result;
        result.reserve(str.size());
        for (char c : str) {
            result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return result;
    }

    bool caseInsensitiveEquals(std::string_view a, std::string_view b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(a[i])) != 
                std::tolower(static_cast<unsigned char>(b[i]))) {
                return false;
            }
        }
        return true;
    }
}

std::string RequestLogger::redactUrl(std::string_view url) {
    std::string result(url);
    
    // Find the query string start
    size_t query_start = result.find('?');
    if (query_start == std::string::npos) {
        return result;  // No query parameters
    }
    
    // Extract base URL and query string
    std::string base_url = result.substr(0, query_start + 1);
    std::string query_string = result.substr(query_start + 1);
    
    // Parse and redact query parameters
    std::istringstream query_stream(query_string);
    std::string param_pair;
    std::vector<std::string> safe_params;
    
    while (std::getline(query_stream, param_pair, '&')) {
        size_t equals_pos = param_pair.find('=');
        if (equals_pos == std::string::npos) {
            // Parameter without value, keep as-is
            safe_params.push_back(param_pair);
            continue;
        }
        
        std::string param_name = param_pair.substr(0, equals_pos);
        std::string param_value = param_pair.substr(equals_pos + 1);
        
        // Check if this is a sensitive parameter
        bool is_sensitive = false;
        std::string lower_name = toLower(param_name);
        for (const auto& sensitive : getSensitiveQueryParamsImpl()) {
            if (lower_name == sensitive) {
                is_sensitive = true;
                break;
            }
        }
        
        if (is_sensitive) {
            // Redact the value
            safe_params.push_back(param_name + "=<REDACTED>");
        } else {
            // Keep the original parameter
            safe_params.push_back(param_pair);
        }
    }
    
    // Reconstruct URL
    result = base_url;
    for (size_t i = 0; i < safe_params.size(); ++i) {
        if (i > 0) result += "&";
        result += safe_params[i];
    }
    
    return result;
}

std::map<std::string, std::string> RequestLogger::redactHeaders(
    const std::map<std::string, std::string>& headers) {
    std::map<std::string, std::string> redacted;
    
    for (const auto& [name, value] : headers) {
        if (isSensitiveHeader(name)) {
            redacted[name] = "<REDACTED>";
        } else {
            redacted[name] = value;
        }
    }
    
    return redacted;
}

std::string RequestLogger::formatRequest(std::string_view method,
                                         std::string_view url,
                                         const std::map<std::string, std::string>& headers) {
    std::ostringstream oss;
    oss << method << " " << redactUrl(url) << "\n";
    oss << "Headers:\n";
    
    auto redacted_headers = redactHeaders(headers);
    for (const auto& [name, value] : redacted_headers) {
        oss << "  " << name << ": " << value << "\n";
    }
    
    return oss.str();
}

const std::vector<std::string>& RequestLogger::getSensitiveQueryParams() {
    return getSensitiveQueryParamsImpl();
}

const std::vector<std::string>& RequestLogger::getSensitiveHeaderNames() {
    return getSensitiveHeaderNamesImpl();
}

bool RequestLogger::isSensitiveHeader(std::string_view header_name) {
    std::string lower_name = toLower(header_name);
    for (const auto& sensitive : getSensitiveHeaderNamesImpl()) {
        if (lower_name == sensitive) {
            return true;
        }
    }
    return false;
}

