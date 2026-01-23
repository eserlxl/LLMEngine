// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LLMEngine/Result.hpp"
#include <cassert>
#include <iostream>
#include <string>

using namespace LLMEngine;

void testTransformError() {
    std::cout << "Testing transformError..." << std::endl;
    Result<int, int> err = Result<int, int>::err(42);

    auto transformed = err.transformError([](int e) { return std::to_string(e); });

    if (!transformed.hasError()) {
        throw std::runtime_error("transformed should have error");
    }
    if (transformed.error() != "42") {
        throw std::runtime_error("transformed error mismatch: expected '42', got "
                                 + transformed.error());
    }
    std::cout << "transformError passed." << std::endl;
}

void testEquality() {
    std::cout << "Testing Equality operators..." << std::endl;
    Result<int, std::string> ok1 = Result<int, std::string>::ok(10);
    Result<int, std::string> ok2 = Result<int, std::string>::ok(10);
    Result<int, std::string> ok3 = Result<int, std::string>::ok(20);
    Result<int, std::string> err1 = Result<int, std::string>::err("fail");
    Result<int, std::string> err2 = Result<int, std::string>::err("fail");
    Result<int, std::string> err3 = Result<int, std::string>::err("buffer");

    if (!(ok1 == ok2))
        throw std::runtime_error("ok1 == ok2 failed");
    if (!(ok1 != ok3))
        throw std::runtime_error("ok1 != ok3 failed");
    if (!(ok1 != err1))
        throw std::runtime_error("ok1 != err1 failed");
    if (!(err1 == err2))
        throw std::runtime_error("err1 == err2 failed");
    if (!(err1 != err3))
        throw std::runtime_error("err1 != err3 failed");

    std::cout << "Equality operators passed." << std::endl;
}

int main() {
    try {
        testTransformError();
        testEquality();
        std::cout << "All Result API tests passed." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}
