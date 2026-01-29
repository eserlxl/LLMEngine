// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of the LLMEngine benchmark suite and is
// licensed under the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/core/ParameterMerger.hpp"

#include <benchmark/benchmark.h>
#include <nlohmann/json.hpp>

static void BM_ParameterMerger_Merge_NoOverrides(benchmark::State& state) {
    nlohmann::json base = {{"temperature", 0.7},
                           {"max_tokens", 1000},
                           {"top_p", 0.9},
                           {"frequency_penalty", 0.0},
                           {"presence_penalty", 0.0}};
    nlohmann::json input = {};

    for (auto _ : state) {
        benchmark::DoNotOptimize(LLMEngine::ParameterMerger::merge(base, input, ""));
    }
}
BENCHMARK(BM_ParameterMerger_Merge_NoOverrides);

static void BM_ParameterMerger_Merge_WithOverrides(benchmark::State& state) {
    nlohmann::json base = {{"temperature", 0.7}, {"max_tokens", 1000}, {"top_p", 0.9}};
    nlohmann::json input = {{"temperature", 0.5}, {"max_tokens", 2000}};

    for (auto _ : state) {
        benchmark::DoNotOptimize(LLMEngine::ParameterMerger::merge(base, input, "chat"));
    }
}
BENCHMARK(BM_ParameterMerger_Merge_WithOverrides);

static void BM_ParameterMerger_MergeInto_NoChanges(benchmark::State& state) {
    nlohmann::json base = {{"temperature", 0.7}, {"max_tokens", 1000}};
    nlohmann::json input = {};
    nlohmann::json out;

    for (auto _ : state) {
        benchmark::DoNotOptimize(LLMEngine::ParameterMerger::mergeInto(base, input, "", out));
    }
}
BENCHMARK(BM_ParameterMerger_MergeInto_NoChanges);

static void BM_ParameterMerger_MergeInto_WithChanges(benchmark::State& state) {
    nlohmann::json base = {{"temperature", 0.7}, {"max_tokens", 1000}};
    nlohmann::json input = {{"temperature", 0.5}};
    nlohmann::json out;

    for (auto _ : state) {
        benchmark::DoNotOptimize(LLMEngine::ParameterMerger::mergeInto(base, input, "", out));
    }
}
BENCHMARK(BM_ParameterMerger_MergeInto_WithChanges);

BENCHMARK_MAIN();
