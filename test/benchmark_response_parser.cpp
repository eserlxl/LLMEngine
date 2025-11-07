// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of the LLMEngine benchmark suite and is
// licensed under the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/ResponseParser.hpp"

#include <benchmark/benchmark.h>
#include <string>

static void BM_ResponseParser_Parse_NoThinkTags(benchmark::State& state) {
    std::string response = "This is a simple response without any think tags. "
                           "It contains just regular content that should be parsed quickly. "
                           "The parser should handle this efficiently without looking for tags.";

    for (auto _ : state) {
        auto result = LLMEngine::ResponseParser::parseResponse(response);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_ResponseParser_Parse_NoThinkTags);

static void BM_ResponseParser_Parse_WithThinkTags(benchmark::State& state) {
    std::string response = "<think>I need to analyze this request carefully. "
                           "The user is asking about a complex topic that requires "
                           "careful consideration of multiple factors.</think>"
                           "Based on my analysis, here is the answer to your question. "
                           "The solution involves several key components that work together.";

    for (auto _ : state) {
        auto result = LLMEngine::ResponseParser::parseResponse(response);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_ResponseParser_Parse_WithThinkTags);

static void BM_ResponseParser_Parse_LongResponse(benchmark::State& state) {
    std::string response;
    response.reserve(10000);
    response = "<think>";
    for (int i = 0; i < 1000; ++i) {
        response += "This is a long reasoning section that contains many words. ";
    }
    response += "</think>";
    for (int i = 0; i < 1000; ++i) {
        response += "This is a long content section with many words as well. ";
    }

    for (auto _ : state) {
        auto result = LLMEngine::ResponseParser::parseResponse(response);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_ResponseParser_Parse_LongResponse);

BENCHMARK_MAIN();
