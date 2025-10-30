// Copyright © 2025 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of the LLMEngine test suite (enum round-trip tests) and is
// licensed under the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "APIClient.hpp"
#include <cassert>
#include <iostream>

using namespace LLMEngineAPI;

static void round_trip(ProviderType t) {
    auto s = APIClientFactory::providerTypeToString(t);
    auto t2 = APIClientFactory::stringToProviderType(s);
    // Allow fallback behavior but ensure known round-trips stay equal
    assert(t2 == t);
}

int main() {
    round_trip(ProviderType::QWEN);
    round_trip(ProviderType::OPENAI);
    round_trip(ProviderType::ANTHROPIC);
    round_trip(ProviderType::OLLAMA);
    std::cout << "test_provider_type_mapping: OK\n";
    return 0;
}


