// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/RequestLogger.hpp"
#include "LLMEngine/Logger.hpp"
#include <algorithm>
#include <ranges>
#include <sstream>
#include <cctype>
#include <unordered_set>
#include "LLMEngine/SensitiveFields.hpp"

namespace {
    // Helper to convert string_view array to lowercase unordered_set for O(1) lookup
    template<size_t N>
    std::unordered_set<std::string> makeLowercaseSet(const std::array<std::string_view, N>& arr) {
        std::unordered_set<std::string> result;
        result.reserve(N);
        for (auto s : arr) {
            std::string lower;
            lower.reserve(s.size());
            for (char c : s) {
                lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            result.insert(std::move(lower));
        }
        return result;
    }

    const std::unordered_set<std::string>& getSensitiveQueryParamsImpl() {
        static const std::unordered_set<std::string> params = 
            makeLowercaseSet(LLMEngine::Security::SENSITIVE_QUERY_PARAMS);
        return params;
    }

    const std::unordered_set<std::string>& getSensitiveHeaderNamesImpl() {
        static const std::unordered_set<std::string> headers = 
            makeLowercaseSet(LLMEngine::Security::SENSITIVE_HEADER_NAMES);
        return headers;
    }

    const std::unordered_set<std::string>& getAllowedLogHeaderNamesImpl() {
        static const std::unordered_set<std::string> headers = 
            makeLowercaseSet(LLMEngine::Security::ALLOWED_LOG_HEADER_NAMES);
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
}

namespace LLMEngine {

std::string RequestLogger::redactUrl(std::string_view url) {
    std::string result(url);
    
    // Find the query string start (before fragment)
    size_t query_start = result.find('?');
    if (query_start == std::string::npos) {
        return result;  // No query parameters
    }
    
    // Find fragment start (if any) to preserve it
    size_t fragment_start = result.find('#', query_start);
    std::string fragment;
    if (fragment_start != std::string::npos) {
        fragment = result.substr(fragment_start);
        result = result.substr(0, fragment_start);
    }
    
    // Extract base URL and query string
    std::string base_url = result.substr(0, query_start + 1);
    std::string query_string = result.substr(query_start + 1);
    
    // Parse and redact query parameters
    // Handle both '&' and ';' as parameter separators (RFC 1866)
    std::vector<std::string> safe_params;
    size_t start = 0;
    
    while (start < query_string.size()) {
        // Find next delimiter (& or ;)
        size_t next_delim = query_string.find_first_of("&;", start);
        size_t param_end = (next_delim == std::string::npos) ? query_string.size() : next_delim;
        
        std::string param_pair = query_string.substr(start, param_end - start);
        
        // Skip empty parameters
        if (param_pair.empty()) {
            start = param_end + 1;
            continue;
        }
        
        // Find equals sign (handle URL-encoded = as %3D)
        size_t equals_pos = param_pair.find('=');
        
        // If no '=', check for URL-encoded '=' (%3D)
        if (equals_pos == std::string::npos) {
            size_t encoded_equals = param_pair.find("%3D");
            if (encoded_equals != std::string::npos) {
                // For simplicity, treat %3D as delimiter if it appears early in param
                // (real URL decoding would require full URL decoder)
                equals_pos = encoded_equals;
                // Note: We don't decode here, just check if it's a delimiter
                // Full URL decoding would require a proper URL decoder library
            }
        }
        
        if (equals_pos == std::string::npos) {
            // Parameter without value, keep as-is
            safe_params.push_back(param_pair);
        } else {
            std::string param_name = param_pair.substr(0, equals_pos);
            
            // Basic URL decoding for parameter name (handle %26 for &, %3D for =)
            // Note: This is a simplified decoder; full decoding would need a proper library
            // For now, we handle the common case of %26 and %3D in names
            std::string decoded_name;
            decoded_name.reserve(param_name.size());
            for (size_t i = 0; i < param_name.size(); ++i) {
                if (param_name[i] == '%' && i + 2 < param_name.size()) {
                    if (param_name.substr(i, 3) == "%26") {
                        decoded_name += '&';
                        i += 2;
                    } else if (param_name.substr(i, 3) == "%3D") {
                        decoded_name += '=';
                        i += 2;
                    } else {
                        decoded_name += param_name[i];
                    }
                } else {
                    decoded_name += param_name[i];
                }
            }
            
            // Check if this is a sensitive parameter (O(1) hash lookup)
            std::string lower_name = toLower(decoded_name);
            bool is_sensitive = getSensitiveQueryParamsImpl().contains(lower_name);
            
            if (is_sensitive) {
                // Redact the value (preserve original param name encoding)
                safe_params.push_back(param_name + "=<REDACTED>");
            } else {
                // Keep the original parameter
                safe_params.push_back(param_pair);
            }
        }
        
        start = param_end + 1;
    }
    
    // Reconstruct URL
    result = base_url;
    for (size_t i = 0; i < safe_params.size(); ++i) {
        if (i > 0) {
            // Use '&' as separator (standard, though ';' is also valid)
            result += "&";
        }
        result += safe_params[i];
    }
    
    // Append fragment if present
    result += fragment;
    
    return result;
}

std::map<std::string, std::string> RequestLogger::redactHeaders(
    const std::map<std::string, std::string>& headers) {
    std::map<std::string, std::string> redacted;
    
    // Default-deny header logging: only include headers on the allowlist.
    // Cache lowercased header names to avoid repeated toLower calls
    for (const auto& [name, value] : headers) {
        const std::string lower_name = toLower(name);
        // O(1) hash lookup for allowlist check
        if (!getAllowedLogHeaderNamesImpl().contains(lower_name)) {
            continue; // omit non-allowed headers entirely
        }
        // O(1) hash lookup for sensitive header check
        if (getSensitiveHeaderNamesImpl().contains(lower_name)) {
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

std::vector<std::string> RequestLogger::getSensitiveQueryParams() {
    const auto& set = getSensitiveQueryParamsImpl();
    return {set.begin(), set.end()};
}

std::vector<std::string> RequestLogger::getSensitiveHeaderNames() {
    const auto& set = getSensitiveHeaderNamesImpl();
    return {set.begin(), set.end()};
}

bool RequestLogger::isSensitiveHeader(std::string_view header_name) {
    std::string lower_name = toLower(header_name);
    // O(1) hash lookup instead of O(n) linear search
    return getSensitiveHeaderNamesImpl().contains(lower_name);
}

std::string RequestLogger::redactText(std::string_view text) {
    // Heuristic redaction of key=value, key: value patterns for sensitive keywords (linear pass)
    static const std::vector<std::string> keywords = {
        "api", "key", "token", "secret", "refresh", "client", "password", "passwd"
    };
    const std::string src(text);
    const std::string lower = toLower(src);
    std::string out;
    out.reserve(src.size());

    size_t i = 0;
    while (i < src.size()) {
        bool matched = false;
        // Try match any keyword at position i
        for (const auto& kw : keywords) {
            if (i + kw.size() <= lower.size() && lower.compare(i, kw.size(), kw) == 0) {
                size_t pos = i + kw.size();
                // Consume identifier tail like _secret or Token etc.
                while (pos < lower.size() && (std::isalnum(static_cast<unsigned char>(lower[pos])) || lower[pos] == '_')) {
                    pos++;
                }
                size_t j = pos;
                while (j < src.size() && std::isspace(static_cast<unsigned char>(src[j]))) j++;
                if (j < src.size() && (src[j] == '=' || src[j] == ':')) {
                    j++;
                    while (j < src.size() && std::isspace(static_cast<unsigned char>(src[j]))) j++;
                    // Emit the part up to value start
                    out.append(src.data() + i, j - i);
                    // Redact value until delimiter; handle simple quoted values
                    size_t v = j;
                    if (v < src.size() && (src[v] == '"' || src[v] == '\'')) {
                        char quote = src[v];
                        v++; // skip opening quote
                        while (v < src.size() && src[v] != quote) {
                            // do not attempt to handle escapes; conservative stop at next quote
                            v++;
                        }
                        if (v < src.size()) v++; // include closing quote
                    } else {
                        while (v < src.size() && !std::isspace(static_cast<unsigned char>(src[v])) && src[v] != ',' && src[v] != ';') {
                            v++;
                        }
                    }
                    out.append("<REDACTED>");
                    i = v;
                    matched = true;
                    break;
                }
            }
        }
        if (!matched) {
            out.push_back(src[i]);
            i++;
        }
    }
    return out;
}

void RequestLogger::logSafe(Logger* logger, LogLevel level, std::string_view message) {
    if (logger) {
        // Automatically redact secrets before logging
        const std::string redacted = redactText(message);
        logger->log(level, redacted);
    }
}

} // namespace LLMEngine
