// Copyright © 2026 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "LLMEngine/AnalysisResult.hpp"
#include <functional>
#include <iostream>
#include <string>

namespace LLMEngine {
namespace StreamUtils {

/**
 * @brief Creates a callback that writes stream content to an output stream (e.g. std::cout).
 * @param os Output stream to write to.
 * @return StreamCallback compatible function.
 */
inline StreamCallback toOStream(std::ostream& os) {
    return [&os](const StreamChunk& chunk) {
        if (!chunk.content.empty()) {
            os << chunk.content << std::flush;
        }
    };
}

/**
 * @brief Creates a callback that accumulates stream content into a string buffer.
 * @param buffer String to append to.
 * @return StreamCallback compatible function.
 */
inline StreamCallback accumulator(std::string& buffer) {
    return [&buffer](const StreamChunk& chunk) {
        if (!chunk.content.empty()) {
            buffer.append(chunk.content);
        }
    };
}

} // namespace StreamUtils
} // namespace LLMEngine
