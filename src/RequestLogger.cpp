// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/RequestLogger.hpp"
#include <algorithm>
#include <ranges>
#include <sstream>
#include <cctype>
#include "LLMEngine/SensitiveFields.hpp"

namespace {
    const std::vector<std::string>& getSensitiveQueryParamsImpl() {
        static const std::vector<std::string> params = []{
            std::vector<std::string> v;
            v.reserve(LLMEngine::Security::SENSITIVE_QUERY_PARAMS.size());
            for (auto s : LLMEngine::Security::SENSITIVE_QUERY_PARAMS) v.emplace_back(s);
            return v;
        }();
        return params;
    }

    const std::vector<std::string>& getSensitiveHeaderNamesImpl() {
        static const std::vector<std::string> headers = []{
            std::vector<std::string> v;
            v.reserve(LLMEngine::Security::SENSITIVE_HEADER_NAMES.size());
            for (auto s : LLMEngine::Security::SENSITIVE_HEADER_NAMES) v.emplace_back(s);
            return v;
        }();
        return headers;
    }

    const std::vector<std::string>& getAllowedLogHeaderNamesImpl() {
        static const std::vector<std::string> headers = []{
            std::vector<std::string> v;
            v.reserve(LLMEngine::Security::ALLOWED_LOG_HEADER_NAMES.size());
            for (auto s : LLMEngine::Security::ALLOWED_LOG_HEADER_NAMES) v.emplace_back(s);
            return v;
        }();
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
        
        // Check if this is a sensitive parameter
        std::string lower_name = toLower(param_name);
        bool is_sensitive = std::ranges::any_of(getSensitiveQueryParamsImpl(),
            [&lower_name](const std::string& sensitive) {
                return lower_name == sensitive;
            });
        
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
    
    // Default-deny header logging: only include headers on the allowlist.
    for (const auto& [name, value] : headers) {
        const std::string lower_name = toLower(name);
        const bool allowed = std::ranges::any_of(getAllowedLogHeaderNamesImpl(),
            [&lower_name](const std::string& allowed_name) {
                return lower_name == allowed_name;
            });
        if (!allowed) {
            continue; // omit non-allowed headers entirely
        }
        if (isSensitiveHeader(lower_name)) {
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
    return std::ranges::any_of(getSensitiveHeaderNamesImpl(),
        [&lower_name](const std::string& sensitive) {
            return lower_name == sensitive;
        });
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

} // namespace LLMEngine
