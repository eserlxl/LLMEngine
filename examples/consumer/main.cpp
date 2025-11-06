#include <LLMEngine/LLMEngine.hpp>
#include <iostream>
#include <nlohmann/json.hpp>

using namespace LLMEngine;

int main() {
    try {
        LLMEngine::LLMEngine engine("ollama", "", "llama2");
        AnalysisResult out = engine.analyze("Say hello.", nlohmann::json::object(), "greeting");
        std::cout << out.content << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
