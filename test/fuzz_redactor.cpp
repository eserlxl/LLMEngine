// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fuzz test target for RequestLogger redaction functions.
// Build with: clang++ -fsanitize=fuzzer,address,undefined -O1 fuzz_redactor.cpp -o fuzz_redactor
// Or use CMake with ENABLE_FUZZ=ON (see test/CMakeLists.txt)

#include "LLMEngine/RequestLogger.hpp"
#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <map>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size == 0 || size > 10000) {  // Reasonable size limit
        return 0;
    }
    
    // Create a string from the fuzzer input
    std::string input(reinterpret_cast<const char*>(data), size);
    
    // Test 1: redactUrl with fuzzed input
    try {
        LLMEngine::RequestLogger::redactUrl(input);
    } catch (...) {
        // Ignore exceptions from malformed input
    }
    
    // Test 2: redactText with fuzzed input
    try {
        LLMEngine::RequestLogger::redactText(input);
    } catch (...) {
        // Ignore exceptions from malformed input
    }
    
    // Test 3: redactHeaders with fuzzed header map
    try {
        std::map<std::string, std::string> headers;
        // Try to parse simple key=value pairs from input
        size_t pos = 0;
        while (pos < input.size() && headers.size() < 50) {  // Limit number of headers
            size_t eq_pos = input.find('=', pos);
            if (eq_pos == std::string::npos) break;
            
            std::string key = input.substr(pos, eq_pos - pos);
            size_t next_pos = input.find('\n', eq_pos);
            if (next_pos == std::string::npos) next_pos = input.size();
            
            std::string value = input.substr(eq_pos + 1, next_pos - eq_pos - 1);
            if (!key.empty() && key.size() < 200 && value.size() < 200) {
                headers[key] = value;
            }
            
            pos = next_pos + 1;
        }
        
        if (!headers.empty()) {
            LLMEngine::RequestLogger::redactHeaders(headers);
        }
    } catch (...) {
        // Ignore exceptions from malformed input
    }
    
    // Test 4: isSensitiveHeader with fuzzed header name
    try {
        if (size < 200) {  // Only test reasonable header names
            LLMEngine::RequestLogger::isSensitiveHeader(input);
        }
    } catch (...) {
        // Ignore exceptions
    }
    
    // Test 5: formatRequest with fuzzed components
    try {
        if (size > 10) {
            size_t split1 = size / 3;
            size_t split2 = 2 * size / 3;
            
            std::string method = input.substr(0, std::min(split1, size_t(10)));
            std::string url = input.substr(split1, split2 - split1);
            
            std::map<std::string, std::string> headers;
            std::string header_data = input.substr(split2);
            
            size_t pos = 0;
            while (pos < header_data.size() && headers.size() < 20) {
                size_t colon = header_data.find(':', pos);
                if (colon == std::string::npos) break;
                
                std::string hkey = header_data.substr(pos, colon - pos);
                size_t nl = header_data.find('\n', colon);
                if (nl == std::string::npos) nl = header_data.size();
                
                std::string hval = header_data.substr(colon + 1, nl - colon - 1);
                if (!hkey.empty() && hkey.size() < 100 && hval.size() < 200) {
                    headers[hkey] = hval;
                }
                pos = nl + 1;
            }
            
            if (!method.empty() && !url.empty()) {
                LLMEngine::RequestLogger::formatRequest(method, url, headers);
            }
        }
    } catch (...) {
        // Ignore exceptions
    }
    
    return 0;
}

