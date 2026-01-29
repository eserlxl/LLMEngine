// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LLMEngine/http/RequestOptions.hpp"
#include <cassert>
#include <iostream>

void testBuilderFluentApi() {
    using namespace LLMEngine;
    
    RequestOptions opts = RequestOptionsBuilder()
        .setTimeout(5000)
        .setMaxRetries(3)
        .addHeader("X-Custom", "Value")
        .setTemperature(0.7)
        .setMaxTokens(100)
        .addStopSequence("STOP")
        .build();

    assert(opts.timeout_ms.has_value());
    assert(*opts.timeout_ms == 5000);
    assert(opts.max_retries.has_value());
    assert(*opts.max_retries == 3);
    assert(opts.extra_headers.count("X-Custom") == 1);
    assert(opts.extra_headers.at("X-Custom") == "Value");
    assert(opts.generation.temperature.has_value());
    assert(*opts.generation.temperature == 0.7);
    assert(opts.generation.max_tokens.has_value());
    assert(*opts.generation.max_tokens == 100);
    assert(opts.generation.stop_sequences.size() == 1);
    assert(opts.generation.stop_sequences[0] == "STOP");
    
    std::cout << "testBuilderFluentApi PASSED\n";
}

void testBuilderDefaults() {
    using namespace LLMEngine;
    
    RequestOptions opts = RequestOptionsBuilder().build();

    assert(!opts.timeout_ms.has_value());
    assert(!opts.max_retries.has_value());
    assert(opts.extra_headers.empty());
    assert(!opts.generation.temperature.has_value());
    
    std::cout << "testBuilderDefaults PASSED\n";
}

int main() {
    testBuilderDefaults();
    testBuilderFluentApi();
    std::cout << "All RequestOptionsBuilder tests passed.\n";
    return 0;
}
