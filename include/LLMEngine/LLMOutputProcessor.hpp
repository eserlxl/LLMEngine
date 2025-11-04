// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#ifndef LLM_OUTPUT_PROCESSOR_HPP
#define LLM_OUTPUT_PROCESSOR_HPP

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "LLMEngineExport.hpp"

/**
 * @brief Parses and queries structured LLM outputs.
 */
class LLMENGINE_EXPORT LLMOutputProcessor {
public:
    /** @brief Construct from a JSON string payload. */
    LLMOutputProcessor(std::string_view jsonContent, bool debug = false);
    /** @brief Get the full raw analysis text. */
    [[nodiscard]] std::string getRawAnalysis() const;
    /** @brief Return a named section's content, if present. */
    [[nodiscard]] std::string getSection(std::string_view title) const;
    /** @brief Extract a list of risks from the analysis. */
    [[nodiscard]] std::vector<std::string> extractRisks() const;
    /** @brief True if critical vulnerabilities are detected. */
    [[nodiscard]] bool hasCriticalVulnerabilities() const;
    /** @brief Summarize findings as JSON. */
    [[nodiscard]] nlohmann::json summarizeFindings() const;
    /** @brief Print all parsed sections (debugging aid). */
    void dumpSections() const;
    /** @brief Access the internal section map. */
    [[nodiscard]] const std::unordered_map<std::string, std::string>& getSections() const;
    /** @brief Enable or disable colored output in dumps. */
    void setColorEnabled(bool enabled);
    /** @brief True if parsing detected errors. */
    [[nodiscard]] bool hasErrors() const;
    /** @brief List available section titles. */
    [[nodiscard]] std::vector<std::string> getAvailableSections() const;

private:
    void parseJson(std::string_view jsonContent);
    void parseSections();
    std::string analysis;
    std::unordered_map<std::string, std::string> sections_;
    bool debug_;
    bool colors_;
};

#endif // LLM_OUTPUT_PROCESSOR_HPP


