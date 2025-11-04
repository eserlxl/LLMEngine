// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/ResponseParser.hpp"
#include <string>
#include <cctype>

namespace LLMEngine {

std::pair<std::string, std::string> ResponseParser::parseResponse(std::string_view response) {
    std::string think_section;
    std::string remaining_section = std::string(response);
    
    constexpr std::string_view tag_open = "<think>";
    constexpr std::string_view tag_close = "</think>";
    
    std::string::size_type open = remaining_section.find(tag_open);
    std::string::size_type close = remaining_section.find(tag_close);
    
    if (open != std::string::npos && close != std::string::npos && close > open) {
        think_section = remaining_section.substr(open + tag_open.length(), close - (open + tag_open.length()));
        std::string before = remaining_section.substr(0, open);
        std::string after = remaining_section.substr(close + tag_close.length());
        remaining_section = before + after;
    }
    
    return {trim(think_section), trim(remaining_section)};
}

std::string ResponseParser::trim(const std::string& s) {
    const char* ws = " \t\n\r";
    std::string::size_type start = s.find_first_not_of(ws);
    if (start == std::string::npos) return {};
    std::string::size_type end = s.find_last_not_of(ws);
    return s.substr(start, end - start + 1);
}

} // namespace LLMEngine

