// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#ifndef LLM_OUTPUT_PROCESSOR_HPP
#define LLM_OUTPUT_PROCESSOR_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>

class LLMOutputProcessor {
public:
    LLMOutputProcessor(const std::string& jsonContent, bool debug = false);
    std::string getRawAnalysis() const;
    std::string getSection(const std::string& title) const;
    std::vector<std::string> extractRisks() const;
    bool hasCriticalVulnerabilities() const;
    [[nodiscard]] nlohmann::json summarizeFindings() const;
    void dumpSections() const;
    [[nodiscard]] const std::unordered_map<std::string, std::string>& getSections() const;
    void setColorEnabled(bool enabled);
    bool hasErrors() const;
    std::vector<std::string> getAvailableSections() const;

private:
    void parseJson(const std::string& jsonContent);
    void parseSections();
    std::string analysis;
    std::unordered_map<std::string, std::string> sections_;
    bool debug_;
    bool colors_;
};

#endif // LLM_OUTPUT_PROCESSOR_HPP 