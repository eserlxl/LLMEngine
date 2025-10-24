#pragma once
#include <string>
#include <vector>
#include "LLMEngine.hpp"

namespace Utils {
    extern std::string TMP_DIR;
    std::vector<std::string> readLines(const std::string& filepath, size_t max_lines = 100);
    std::vector<std::string> execCommand(const std::string& cmd);
    std::string stripMarkdown(const std::string& input);
} 