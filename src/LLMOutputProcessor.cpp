#include "LLMOutputProcessor.hpp"
#include <regex>
#include <sstream>
#include <nlohmann/json.hpp>
#include <iostream>
#include <unordered_set>
#include <unistd.h>
#include <cstdio>
#include <algorithm>
#include <cctype>

namespace {
std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \n\r\t");
    auto end = s.find_last_not_of(" \n\r\t");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

std::string lower(const std::string& s) {
    std::string res = s;
    std::transform(res.begin(), res.end(), res.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return res;
}

namespace Color {
    constexpr const char* Red = "\033[31m";
    constexpr const char* Green = "\033[32m";
    constexpr const char* Yellow = "\033[33m";
    constexpr const char* Reset = "\033[0m";
}
}

LLMOutputProcessor::LLMOutputProcessor(const std::string& jsonContent, bool debug)
    : debug_(debug) {
    colors_ = isatty(fileno(stdout));
    parseJson(jsonContent);
    parseSections();
}

std::string LLMOutputProcessor::getRawAnalysis() const {
    return analysis;
}

std::string LLMOutputProcessor::getSection(const std::string& title) const {
    auto it = sections_.find(lower(title));
    if (it != sections_.end()) {
        return trim(it->second);
    }
    return "";
}

std::vector<std::string> LLMOutputProcessor::extractRisks() const {
    std::regex riskRegex(R"(\b(risk|vulnerab(ility|ilities)|exploit|threat)\b)", std::regex::icase);
    std::stringstream ss(analysis);
    std::string line;
    std::vector<std::string> risks;

    while (std::getline(ss, line)) {
        if (std::regex_search(line, riskRegex)) {
            risks.push_back(line);
        }
    }
    return risks;
}

bool LLMOutputProcessor::hasCriticalVulnerabilities() const {
    std::regex critRegex(R"(high[- ]?risk|vulnerab(ility|ilities))", std::regex::icase);
    return std::regex_search(analysis, critRegex);
}

bool LLMOutputProcessor::hasErrors() const {
    return analysis.rfind("[ERROR]", 0) == 0;
}

void LLMOutputProcessor::parseJson(const std::string& jsonContent) {
    try {
        // Handle streaming JSON format from Ollama (multiple JSON objects, one per line)
        std::stringstream ss(jsonContent);
        std::string line;
        std::stringstream fullResponse;
        
        while (std::getline(ss, line)) {
            if (line.empty()) continue;
            
            try {
                auto j = nlohmann::json::parse(line);
                
                // Check if this is an Ollama streaming response format
                if (j.contains("response")) {
                    std::string response = j["response"].get<std::string>();
                    fullResponse << response;
                }
                // Check if this is a traditional format with "data" field
                else if (j.contains("data")) {
                    analysis = j["data"].get<std::string>();
                    return; // Use the data field and return
                }
                // Check if this is a direct analysis field
                else if (j.contains("analysis")) {
                    analysis = j["analysis"].get<std::string>();
                    return; // Use the analysis field and return
                }
            } catch (const std::exception& parseError) {
                if (debug_) std::cerr << "[LLMOutputProcessor] Line parse error: " << parseError.what() << '\n';
                continue; // Skip invalid lines
            }
        }
        
        // If we collected streaming responses, use them
        if (fullResponse.str().empty()) {
            analysis = "[ERROR] No valid response data found in JSON.";
        } else {
            analysis = fullResponse.str();
        }
        
    } catch (const std::exception& e) {
        if (debug_) std::cerr << "[LLMOutputProcessor] JSON parse error: " << e.what() << '\n';
        analysis = "[ERROR] Invalid JSON format.";
    }
}

void LLMOutputProcessor::parseSections() {
    std::regex sectionRegex(R"(^#{2,}\s+\*{0,2}(.*?)\*{0,2}\s*$)", std::regex::icase);
    std::stringstream ss(analysis);
    std::string line;
    std::string currentTitle;
    std::stringstream currentContent;

    while (std::getline(ss, line)) {
        std::smatch match;
        if (std::regex_match(line, match, sectionRegex)) {
            if (!currentTitle.empty()) {
                sections_[lower(currentTitle)] = currentContent.str();
                currentContent.str("");
                currentContent.clear();
            }
            currentTitle = match[1];
        } else if (!currentTitle.empty()) {
            currentContent << line << '\n';
        }
    }
    if (!currentTitle.empty()) {
        sections_[lower(currentTitle)] = currentContent.str();
    }
}

void LLMOutputProcessor::dumpSections() const {
    for (const auto& [key, content] : sections_) {
        std::cout << "Section: " << key << "\n" << content << "\n---\n" << std::endl;
    }
}

nlohmann::json LLMOutputProcessor::summarizeFindings() const {
    nlohmann::json summary;

    auto risks = extractRisks();

    if (hasCriticalVulnerabilities()) {
        summary["severity"] = "critical";
    } else if (!risks.empty()) {
        summary["severity"] = "warning";
    } else {
        summary["severity"] = "info";
    }

    std::string conclusion = getSection("Conclusion");
    if (!conclusion.empty()) {
        summary["conclusion"] = conclusion;
    }

    if (!risks.empty()) {
        summary["risks"] = nlohmann::json::array();
        std::unordered_set<std::string> seen;
        for (const auto& risk : risks) {
            std::string clean_risk = trim(std::regex_replace(risk, std::regex("^-*\\s*"), ""));
            if (seen.insert(clean_risk).second) {
                summary["risks"].push_back(clean_risk);
            }
        }
    }

    std::string nextSteps = getSection("Next Steps");
    if (!nextSteps.empty()) {
        summary["suggestions"] = nlohmann::json::array();
        std::stringstream ss(nextSteps);
        std::string item;
        while (std::getline(ss, item, '\n')) {
            if (!item.empty()) {
                summary["suggestions"].push_back(trim(std::regex_replace(item, std::regex("^-*\\s*"), "")));
            }
        }
    }
    
    if (summary.value("severity", "info") == "info" &&
        !summary.contains("risks") &&
        !summary.contains("suggestions") &&
        !summary.contains("conclusion")) {
        return nlohmann::json::object(); 
    }

    if (!summary.contains("conclusion")) {
        if (summary.value("severity", "") == "info") {
            summary["conclusion"] = "No critical issues found.";
        } else {
            summary["conclusion"] = "Review detected risks and apply suggestions.";
        }
    }

    return summary;
}

const std::unordered_map<std::string, std::string>& LLMOutputProcessor::getSections() const {
    return sections_;
}

void LLMOutputProcessor::setColorEnabled(bool enabled) {
    colors_ = enabled;
}

std::vector<std::string> LLMOutputProcessor::getAvailableSections() const {
    std::vector<std::string> titles;
    titles.reserve(sections_.size());
    for (const auto& [title, _] : sections_) {
        titles.push_back(title);
    }
    std::sort(titles.begin(), titles.end());
    return titles;
}
