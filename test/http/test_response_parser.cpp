// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of the LLMEngine test suite (ResponseParser tests) and is
// licensed under the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine/http/ResponseParser.hpp"

#include <cassert>
#include <iostream>
#include <string>

using namespace LLMEngine;

void testBasicParsing() {
    std::string response = "Some text <think>thinking here</think> more text";
    auto [think, content] = ResponseParser::parseResponse(response);
    assert(think == "thinking here");
    assert(content == "Some text  more text");
    std::cout << "✓ Basic parsing test passed\n";
}

void testNoTags() {
    std::string response = "Just regular text without any tags";
    auto [think, content] = ResponseParser::parseResponse(response);
    assert(think.empty());
    assert(content == "Just regular text without any tags");
    std::cout << "✓ No tags test passed\n";
}

void testEmptyThinkSection() {
    std::string response = "Text <think></think> more text";
    auto [think, content] = ResponseParser::parseResponse(response);
    assert(think.empty());
    assert(content == "Text  more text");
    std::cout << "✓ Empty think section test passed\n";
}

void testOnlyThinkSection() {
    std::string response = "<think>only thinking</think>";
    auto [think, content] = ResponseParser::parseResponse(response);
    assert(think == "only thinking");
    assert(content.empty());
    std::cout << "✓ Only think section test passed\n";
}

void testMultipleTags() {
    // Should only parse the first occurrence
    std::string response = "<think>first</think> text <think>second</think>";
    auto [think, content] = ResponseParser::parseResponse(response);
    assert(think == "first");
    // Content is trimmed, so leading space is removed
    assert(content == "text <think>second</think>");
    std::cout << "✓ Multiple tags test passed\n";
}

void testUnclosedTag() {
    std::string response = "Text <think>unclosed thinking";
    auto [think, content] = ResponseParser::parseResponse(response);
    assert(think.empty());
    assert(content == "Text <think>unclosed thinking");
    std::cout << "✓ Unclosed tag test passed\n";
}

void testUnopenedTag() {
    std::string response = "Text </think> more text";
    auto [think, content] = ResponseParser::parseResponse(response);
    assert(think.empty());
    assert(content == "Text </think> more text");
    std::cout << "✓ Unopened tag test passed\n";
}

void testWhitespaceTrimming() {
    std::string response = "  <think>  thinking with spaces  </think>  content with spaces  ";
    auto [think, content] = ResponseParser::parseResponse(response);
    assert(think == "thinking with spaces");
    assert(content == "content with spaces");
    std::cout << "✓ Whitespace trimming test passed\n";
}

void testNewlinesAndTabs() {
    std::string response = "\t\n<think>\n\tthinking\n\t</think>\n\tcontent\n\t";
    auto [think, content] = ResponseParser::parseResponse(response);
    assert(think == "thinking");
    assert(content == "content");
    std::cout << "✓ Newlines and tabs test passed\n";
}

void testEmptyResponse() {
    std::string response = "";
    auto [think, content] = ResponseParser::parseResponse(response);
    assert(think.empty());
    assert(content.empty());
    std::cout << "✓ Empty response test passed\n";
}

void testWhitespaceOnly() {
    std::string response = "   \t\n   ";
    auto [think, content] = ResponseParser::parseResponse(response);
    assert(think.empty());
    assert(content.empty());
    std::cout << "✓ Whitespace only test passed\n";
}

void testTagAtStart() {
    std::string response = "<think>think</think>content";
    auto [think, content] = ResponseParser::parseResponse(response);
    assert(think == "think");
    assert(content == "content");
    std::cout << "✓ Tag at start test passed\n";
}

void testTagAtEnd() {
    std::string response = "content<think>think</think>";
    auto [think, content] = ResponseParser::parseResponse(response);
    assert(think == "think");
    assert(content == "content");
    std::cout << "✓ Tag at end test passed\n";
}

int main() {
    std::cout << "=== ResponseParser Unit Tests ===\n";

    testBasicParsing();
    testNoTags();
    testEmptyThinkSection();
    testOnlyThinkSection();
    testMultipleTags();
    testUnclosedTag();
    testUnopenedTag();
    testWhitespaceTrimming();
    testNewlinesAndTabs();
    testEmptyResponse();
    testWhitespaceOnly();
    testTagAtStart();
    testTagAtEnd();

    std::cout << "\nAll ResponseParser tests passed!\n";
    return 0;
}
