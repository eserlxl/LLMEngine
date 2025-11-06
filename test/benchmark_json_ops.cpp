// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of the LLMEngine benchmark suite and is
// licensed under the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include <benchmark/benchmark.h>
#include <nlohmann/json.hpp>

static void BM_JSON_Merge_Empty(benchmark::State& state) {
    nlohmann::json base = {
        {"temperature", 0.7},
        {"max_tokens", 1000}
    };
    nlohmann::json override_json = {};
    
    for (auto _ : state) {
        nlohmann::json result = base;
        result.update(override_json);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_JSON_Merge_Empty);

static void BM_JSON_Merge_WithOverrides(benchmark::State& state) {
    nlohmann::json base = {
        {"temperature", 0.7},
        {"max_tokens", 1000},
        {"top_p", 0.9}
    };
    nlohmann::json override_json = {
        {"temperature", 0.5},
        {"max_tokens", 2000}
    };
    
    for (auto _ : state) {
        nlohmann::json result = base;
        result.update(override_json);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_JSON_Merge_WithOverrides);

static void BM_JSON_Parse_Simple(benchmark::State& state) {
    std::string json_str = R"({"temperature": 0.7, "max_tokens": 1000})";
    
    for (auto _ : state) {
        auto result = nlohmann::json::parse(json_str);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_JSON_Parse_Simple);

static void BM_JSON_Parse_Complex(benchmark::State& state) {
    std::string json_str = R"({
        "messages": [
            {"role": "system", "content": "You are a helpful assistant."},
            {"role": "user", "content": "What is 2+2?"}
        ],
        "model": "gpt-4",
        "temperature": 0.7,
        "max_tokens": 1000,
        "top_p": 0.9,
        "frequency_penalty": 0.0,
        "presence_penalty": 0.0
    })";
    
    for (auto _ : state) {
        auto result = nlohmann::json::parse(json_str);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_JSON_Parse_Complex);

static void BM_JSON_Serialize(benchmark::State& state) {
    nlohmann::json json_obj = {
        {"messages", nlohmann::json::array({
            {{"role", "system"}, {"content", "You are a helpful assistant."}},
            {{"role", "user"}, {"content", "What is 2+2?"}}
        })},
        {"model", "gpt-4"},
        {"temperature", 0.7},
        {"max_tokens", 1000}
    };
    
    for (auto _ : state) {
        std::string result = json_obj.dump();
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_JSON_Serialize);

BENCHMARK_MAIN();

