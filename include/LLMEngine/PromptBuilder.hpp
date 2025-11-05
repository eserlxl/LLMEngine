// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of LLMEngine and is licensed under
// the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#pragma once
#include <string>
#include <string_view>
#include "LLMEngine/LLMEngineExport.hpp"

namespace LLMEngine {

/**
 * @brief Strategy interface for building prompts.
 * 
 * Allows different prompt building strategies (terse, verbose, custom, etc.)
 * to be composed into the analysis pipeline.
 */
class LLMENGINE_EXPORT IPromptBuilder {
public:
    virtual ~IPromptBuilder() = default;
    
    /**
     * @brief Build the final prompt from the user-provided prompt.
     * 
     * @param prompt User-provided prompt
     * @return Final prompt to send to the LLM
     */
    [[nodiscard]] virtual std::string buildPrompt(std::string_view prompt) const = 0;
};

/**
 * @brief Default prompt builder that prepends a terse instruction.
 * 
 * Prepends: "Please respond directly to the previous message, engaging with its content. 
 *            Try to be brief and concise and complete your response in one or two sentences, 
 *            mostly one sentence.\n"
 * 
 * The system instruction is cached as a static constant to avoid repeated allocations.
 */
class LLMENGINE_EXPORT TersePromptBuilder : public IPromptBuilder {
public:
    [[nodiscard]] std::string buildPrompt(std::string_view prompt) const override {
        // Cache the system instruction as a static constant to avoid repeated allocations
        static const std::string system_instruction =
            "Please respond directly to the previous message, engaging with its content. "
            "Try to be brief and concise and complete your response in one or two sentences, "
            "mostly one sentence.\n";
        // Pre-allocate result to avoid reallocations
        std::string result;
        result.reserve(system_instruction.size() + prompt.size());
        result = system_instruction;
        result += prompt;
        return result;
    }
};

/**
 * @brief Pass-through prompt builder that returns the prompt unchanged.
 * 
 * Useful for evaluation or when precise prompt control is needed.
 */
class LLMENGINE_EXPORT PassthroughPromptBuilder : public IPromptBuilder {
public:
    [[nodiscard]] std::string buildPrompt(std::string_view prompt) const override {
        return std::string(prompt);
    }
};

} // namespace LLMEngine

