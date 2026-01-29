// Copyright © 2026 Eser KUBALI
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LLMEngine/utils/Logger.hpp"
#include "LLMEngine/utils/StreamUtils.hpp"
#include <cassert>
#include <sstream>
#include <string>
#include <vector>

using namespace LLMEngine;

void test_stream_utils() {
    std::cout << "Testing StreamUtils..." << std::endl;

    // Test accumulator
    std::string buffer;
    auto accKey = StreamUtils::accumulator(buffer);
    StreamChunk c1; c1.content = "Hello";
    StreamChunk c2; c2.content = " World";
    
    accKey(c1);
    accKey(c2);
    
    assert(buffer == "Hello World");

    // Test toOStream
    std::stringstream ss;
    auto streamKey = StreamUtils::toOStream(ss);
    
    streamKey(c1);
    streamKey(c2);
    
    assert(ss.str() == "Hello World");
    
    std::cout << "PASS" << std::endl;
}

void test_callback_logger() {
    std::cout << "Testing CallbackLogger..." << std::endl;
    
    std::vector<std::string> logs;
    auto callback = [&](LogLevel level, std::string_view msg) {
        logs.push_back(std::string(msg));
    };
    
    auto logger = std::make_shared<CallbackLogger>(callback);
    
    logger->log(LogLevel::Info, "Log 1");
    logger->log(LogLevel::Error, "Log 2");
    
    assert(logs.size() == 2);
    assert(logs[0] == "Log 1");
    assert(logs[1] == "Log 2");
    
    std::cout << "PASS" << std::endl;
}

int main() {
    try {
        test_stream_utils();
        test_callback_logger();
        std::cout << "All Iteration 3 feature tests passed." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
