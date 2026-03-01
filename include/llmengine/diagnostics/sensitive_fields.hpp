// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Centralized lists of sensitive parameter and header names for redaction.

#pragma once
#include <array>
#include <string_view>

namespace LLMEngine::Security {

// Query parameter names to redact (case-insensitive exact match via hash lookup)
// Implementation uses O(1) hash-based lookup for precise matching to reduce false positives
inline constexpr std::array<std::string_view, 14> SENSITIVE_QUERY_PARAMS = {"key",
                                                                            "api_key",
                                                                            "apikey",
                                                                            "x-api-key",
                                                                            "xapikey",
                                                                            "token",
                                                                            "access_token",
                                                                            "accesstoken",
                                                                            "secret",
                                                                            "password",
                                                                            "passwd",
                                                                            "pwd",
                                                                            "credential",
                                                                            "authorization"};

// Header names to redact (case-insensitive exact match via hash lookup)
// Implementation uses O(1) hash-based lookup for precise matching to reduce false positives
inline constexpr std::array<std::string_view, 14> SENSITIVE_HEADER_NAMES = {"authorization",
                                                                            "proxy-authorization",
                                                                            "x-authorization",
                                                                            "x-api-key",
                                                                            "xapikey",
                                                                            "api-key",
                                                                            "x-goog-api-key",
                                                                            "x-auth-token",
                                                                            "access-token",
                                                                            "secret",
                                                                            "password",
                                                                            "cookie",
                                                                            "set-cookie",
                                                                            "credential"};

// Header names allowed to be logged verbatim (case-insensitive exact match)
// All other headers should be omitted or redacted from logs
inline constexpr std::array<std::string_view, 6> ALLOWED_LOG_HEADER_NAMES = {
    "content-type", "accept", "user-agent", "content-length", "accept-encoding", "connection"};

} // namespace LLMEngine::Security
