// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/ResponseParser.hpp"

#include <cctype>
#include <string>

namespace LLMEngine {

std::pair<std::string, std::string> ResponseParser::parseResponse(std::string_view response) {
    constexpr std::string_view tag_open = "<think>";
    constexpr std::string_view tag_close = "</think>";

    const std::size_t open = response.find(tag_open);
    const std::size_t close =
        response.find(tag_close, open == std::string_view::npos ? 0 : open + tag_open.size());

    std::string think_section;
    std::string remaining_section;

    if (open != std::string_view::npos && close != std::string_view::npos && close > open) {
        std::string_view think_sv =
            response.substr(open + tag_open.size(), close - (open + tag_open.size()));
        std::string_view before_sv = response.substr(0, open);
        std::string_view after_sv = response.substr(close + tag_close.size());
        think_section.assign(think_sv.begin(), think_sv.end());
        remaining_section.reserve(before_sv.size() + after_sv.size());
        remaining_section.append(before_sv.begin(), before_sv.end());
        remaining_section.append(after_sv.begin(), after_sv.end());
    } else {
        remaining_section.assign(response.begin(), response.end());
    }

    return {trim(think_section), trim(remaining_section)};
}

std::string ResponseParser::trim(const std::string& s) {
    const char* ws = " \t\n\r";
    std::string::size_type start = s.find_first_not_of(ws);
    if (start == std::string::npos)
        return {};
    std::string::size_type end = s.find_last_not_of(ws);
    return s.substr(start, end - start + 1);
}

} // namespace LLMEngine
