#include "LLMEngine/AnalysisResult.hpp"
#include <iostream>
#include <cassert>
#include <nlohmann/json.hpp>

using namespace LLMEngine;

int main() {
    std::cout << "Testing AnalysisResult Extensions...\n";

    // Plain JSON
    {
        AnalysisResult result;
        result.content = R"({"key": "value"})";
        result.success = true;
        auto json = result.getJson();
        assert(json.has_value());
        assert((*json)["key"] == "value");
    }

    // Markdown
    {
        AnalysisResult result;
        result.content = "Here is the result:\n```json\n{\"foo\": 123}\n```";
        result.success = true;
        auto json = result.getJson();
        assert(json.has_value());
        assert((*json)["foo"] == 123);
    }

    // Markdown without language
    {
        AnalysisResult result;
        result.content = "```\n{\"bar\": true}\n```";
        result.success = true;
        auto json = result.getJson();
        assert(json.has_value());
        assert((*json)["bar"] == true);
    }

    // Invalid JSON
    {
        AnalysisResult result;
        result.content = "Not JSON";
        result.success = true;
        auto json = result.getJson();
        assert(!json.has_value());
    }

    std::cout << "AnalysisResult tests passed.\n";
    return 0;
}
