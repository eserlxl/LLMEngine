#include "LLMEngine/LLMOutputProcessor.hpp"
#include <cassert>
#include <iostream>

static std::string build_streaming_json() {
    std::string out;
    out += std::string("{") + "\"response\": \"## Summary\\nAll good.\\n\"}";
    out += std::string("{") + "\"response\": \"## Conclusion\\nShip it.\\n\"}";
    return out;
}

int main() {
    const std::string payload = build_streaming_json();
    LLMOutputProcessor proc(payload, /*debug=*/false);

    // Raw analysis should contain both sections text
    auto raw = proc.getRawAnalysis();
    assert(raw.find("All good.") != std::string::npos);
    assert(raw.find("Ship it.") != std::string::npos);

    // Section extraction must be case-insensitive and trimmed
    auto summary = proc.getSection("summary");
    auto conclusion = proc.getSection("Conclusion");
    assert(summary.find("All good.") != std::string::npos);
    assert(conclusion.find("Ship it.") != std::string::npos);

    // Summarize should at least produce an object (may be empty fields based on heuristics)
    auto js = proc.summarizeFindings();
    (void)js; // best-effort contract

    std::cout << "OK\n";
    return 0;
}


