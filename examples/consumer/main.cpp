#include <LLMEngine/LLMEngine.hpp>
#include <nlohmann/json.hpp>
#include <iostream>

using namespace LLMEngine;

int main() {
    try {
        LLMEngine engine("ollama", "", "llama2");
        AnalysisResult out = engine.analyze("Say hello.", nlohmann::json::object(), "greeting");
        std::cout << out.content << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}


