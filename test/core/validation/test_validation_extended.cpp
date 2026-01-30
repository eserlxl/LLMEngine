// Copyright © 2026 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LLMEngine/core/AnalysisInput.hpp"
#include <gtest/gtest.h>

using namespace LLMEngine;

class AnalysisInputValidationTest : public ::testing::Test {};

TEST_F(AnalysisInputValidationTest, EmptyInputFailsValidation) {
    auto input = AnalysisInput::builder();
    std::string error;
    EXPECT_FALSE(input.validate(error));
    EXPECT_EQ(error, "AnalysisInput must contain at least one message (system_prompt, user_message, or messages)");
}

TEST_F(AnalysisInputValidationTest, SystemPromptOnlyPasses) {
    auto input = AnalysisInput::builder().withSystemPrompt("Act as a helpful assistant");
    std::string error;
    EXPECT_TRUE(input.validate(error)) << "Error: " << error;
}

TEST_F(AnalysisInputValidationTest, UserMessageOnlyPasses) {
    auto input = AnalysisInput::builder().withUserMessage("Hello world");
    std::string error;
    EXPECT_TRUE(input.validate(error)) << "Error: " << error;
}

TEST_F(AnalysisInputValidationTest, ManualMessagesPasses) {
    auto input = AnalysisInput::builder()
        .withMessage("user", "Hello");
    std::string error;
    EXPECT_TRUE(input.validate(error)) << "Error: " << error;
}

TEST_F(AnalysisInputValidationTest, ToolsAndToolChoiceTypes) {
    // This existing logic should still work
    auto input = AnalysisInput::builder()
        .withUserMessage("Use tool")
        .withToolChoice(ToolChoice::autoChoice());
        
    std::string error;
    EXPECT_TRUE(input.validate(error)); // Should pass as it has user message
}
