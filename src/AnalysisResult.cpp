#include "LLMEngine/AnalysisResult.hpp"
#include <regex>

namespace LLMEngine {

std::optional<nlohmann::json> AnalysisResult::getJson() const {
    if (content.empty()) {
        return std::nullopt;
    }

    std::string clean_content = content;

    // Simple markdown code block stripping
    // Try to find ```json ... ``` or just ``` ... ``` and extract key part
    static const std::regex code_block_regex(R"(```(?:json)?\s*([\s\S]*?)\s*```)");
    std::smatch match;
    if (std::regex_search(content, match, code_block_regex)) {
        if (match.size() > 1) {
            clean_content = match.str(1);
        }
    }

    // Try to parse
    try {
        return nlohmann::json::parse(clean_content);
    } catch (...) {
        // If parsing fails, try to find the first { and last } to handle preamble/postscript without code blocks
        size_t first_brace = clean_content.find('{');
        size_t last_brace = clean_content.rfind('}');
        
        if (first_brace != std::string::npos && last_brace != std::string::npos && last_brace > first_brace) {
            std::string potential_json = clean_content.substr(first_brace, last_brace - first_brace + 1);
            try {
                return nlohmann::json::parse(potential_json);
            } catch (...) {
                return std::nullopt;
            }
        }
        
        return std::nullopt;
    }
}

} // namespace LLMEngine
