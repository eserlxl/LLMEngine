#include <LLMEngine/LLMEngine.hpp>
#include <nlohmann/json.hpp>
#include <iostream>

int main() {
    try {
        LLMEngine engine("ollama", "", "llama2");
        auto out = engine.analyze("Say hello.", nlohmann::json::object(), "greeting");
        std::cout << out[1] << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}


