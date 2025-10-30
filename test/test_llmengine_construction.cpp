// Copyright © 2025 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LLMEngine.hpp"
#include "support/FakeAPIClient.hpp"
#include <cassert>
#include <iostream>

using namespace LLMEngineAPI;

int main() {
    // Construct with DI client
    auto fake = std::make_unique<FakeAPIClient>(ProviderType::OPENAI, "FakeOpenAI");
    LLMEngine engine(std::move(fake), /*model_params*/{}, /*log_retention_hours*/1, /*debug*/false);

    // Smoke test analyze (uses FakeAPIClient)
    auto out = engine.analyze("hello", {}, "unittest", "chat");
    assert(out.size() == 2);
    // The second element is the visible content
    assert(out[1].find("[FAKE]") != std::string::npos);

    // Legacy constructors still compile and construct; avoid network by not calling analyze
    LLMEngine legacy("http://localhost:11434", "llama2");
    (void)legacy;

    std::cout << "test_llmengine_construction: OK\n";
    return 0;
}


