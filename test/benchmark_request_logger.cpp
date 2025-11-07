// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of the LLMEngine benchmark suite and is
// licensed under the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/RequestLogger.hpp"

#include <benchmark/benchmark.h>
#include <map>
#include <string>

static void BM_RequestLogger_RedactText_NoSecrets(benchmark::State& state) {
    std::string text = "This is a normal log message without any secrets. "
                       "It contains regular text that should not be redacted. "
                       "The redaction function should handle this efficiently.";

    for (auto _ : state) {
        benchmark::DoNotOptimize(LLMEngine::RequestLogger::redactText(text));
    }
}
BENCHMARK(BM_RequestLogger_RedactText_NoSecrets);

static void BM_RequestLogger_RedactText_WithSecrets(benchmark::State& state) {
    std::string text = "api_key=sk_abcdefghijklmnopqrstuvwxyz0123456789 "
                       "token=Bearer xyz123456789 "
                       "secret=my_secret_value_here";

    for (auto _ : state) {
        benchmark::DoNotOptimize(LLMEngine::RequestLogger::redactText(text));
    }
}
BENCHMARK(BM_RequestLogger_RedactText_WithSecrets);

static void BM_RequestLogger_RedactUrl_NoSensitiveParams(benchmark::State& state) {
    std::string url = "https://api.example.com/v1/chat?model=gpt-4&temperature=0.7&max_tokens=1000";

    for (auto _ : state) {
        benchmark::DoNotOptimize(LLMEngine::RequestLogger::redactUrl(url));
    }
}
BENCHMARK(BM_RequestLogger_RedactUrl_NoSensitiveParams);

static void BM_RequestLogger_RedactUrl_WithSensitiveParams(benchmark::State& state) {
    std::string url = "https://api.example.com/v1/chat?api_key=sk_test123&token=abc456&model=gpt-4";

    for (auto _ : state) {
        benchmark::DoNotOptimize(LLMEngine::RequestLogger::redactUrl(url));
    }
}
BENCHMARK(BM_RequestLogger_RedactUrl_WithSensitiveParams);

static void BM_RequestLogger_RedactHeaders(benchmark::State& state) {
    std::map<std::string, std::string> headers = {
        {"Authorization", "Bearer sk_abcdefghijklmnopqrstuvwxyz012345"},
        {"Content-Type", "application/json"},
        {"X-API-Key", "test_key_12345"},
        {"User-Agent", "LLMEngine/1.0"}};

    for (auto _ : state) {
        benchmark::DoNotOptimize(LLMEngine::RequestLogger::redactHeaders(headers));
    }
}
BENCHMARK(BM_RequestLogger_RedactHeaders);

BENCHMARK_MAIN();
