// Copyright © 2026 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of the LLMEngine test suite (LLMEngine construction tests)
// and is licensed under the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/core/LLMEngine.hpp"
#include "support/FakeAPIClient.hpp"

#include <cassert>
#include <iostream>

using namespace LLMEngineAPI;
using namespace LLMEngine;

int main() {
    // Construct with DI client
    auto fake = std::make_unique<FakeAPIClient>(ProviderType::OPENAI, "FakeOpenAI");
    ::LLMEngine::LLMEngine engine(std::move(fake),
                                  /*model_params*/ {},
                                  /*log_retention_hours*/ 1,
                                  /*debug*/ false);

    // Smoke test analyze (uses FakeAPIClient)
    AnalysisResult out = engine.analyze("hello", {}, "unittest", "chat");
    assert(out.success);
    // Visible content contains fake marker
    assert(out.content.find("[FAKE]") != std::string::npos);

    // Test Ollama provider via ProviderType constructor
    ::LLMEngine::LLMEngine ollama_engine(ProviderType::OLLAMA, "", "llama2");
    (void)ollama_engine;

    std::cout << "test_llmengine_construction: OK\n";
    return 0;
}
