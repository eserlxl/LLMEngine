// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This example showcases text processing/summarization with LLMEngine and is
// licensed under the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine.hpp"
#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <ctime>

class TextProcessor {
private:
    std::unique_ptr<LLMEngine> engine_;
    bool debug_mode_;
    std::string mode_;
    
public:
    TextProcessor(const std::string& provider_name, const std::string& api_key, 
                  const std::string& model = "", bool debug = false, const std::string& mode = "chat") 
        : debug_mode_(debug), mode_(mode) {
        try {
            // Configure parameters optimized for text processing
            nlohmann::json text_params = {
                {"temperature", 0.5},        // Balanced for text processing tasks
                {"max_tokens", 3000},         // Good length for summaries and analysis
                {"top_p", 0.9},              // Good balance for text tasks
                {"frequency_penalty", 0.1},   // Slight penalty to avoid repetition
                {"presence_penalty", 0.0}     // No penalty for introducing concepts
            };
            
            engine_ = std::make_unique<LLMEngine>(provider_name, api_key, model, 
                                                 text_params, 24, debug);
            std::cout << "✓ TextProcessor initialized with " << engine_->getProviderName() 
                      << " (" << (engine_->isOnlineProvider() ? "Online" : "Local") << ")" 
                      << " in " << mode_ << " mode" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "❌ Failed to initialize TextProcessor: " << e.what() << std::endl;
            throw;
        }
    }
    
    void summarizeText(const std::string& text, const std::string& output_file = "") {
        if (text.empty()) {
            std::cerr << "❌ No text provided for summarization!" << std::endl;
            return;
        }
        
        std::cout << "\n📝 Summarizing text (" << text.length() << " characters)..." << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        
        try {
            std::string prompt = "Please provide a comprehensive summary of the following text. Include the main points, key insights, and important details. Make it clear and well-structured.";
            nlohmann::json input = {{"text", text}};
            
            AnalysisResult result = engine_->analyze(prompt, input, "summarization", mode_);
            std::string summary = result.content;
            
            std::cout << "📋 Summary:" << std::endl;
            std::cout << summary << std::endl;
            
            if (!output_file.empty()) {
                saveToFile(output_file, summary, "Summary");
            }
            
        } catch (const std::exception& e) {
            std::cerr << "❌ Error during summarization: " << e.what() << std::endl;
        }
    }
    
    void summarizeFile(const std::string& filepath, const std::string& output_file = "") {
        if (!std::filesystem::exists(filepath)) {
            std::cerr << "❌ File not found: " << filepath << std::endl;
            return;
        }
        
        std::string content = readFile(filepath);
        if (content.empty()) {
            std::cerr << "❌ Failed to read file: " << filepath << std::endl;
            return;
        }
        
        std::cout << "\n📄 Summarizing file: " << filepath << std::endl;
        std::cout << "File size: " << content.length() << " characters" << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        
        summarizeText(content, output_file);
    }
    
    void extractKeywords(const std::string& text, int max_keywords = 10) {
        if (text.empty()) {
            std::cerr << "❌ No text provided for keyword extraction!" << std::endl;
            return;
        }
        
        std::cout << "\n🔑 Extracting keywords from text..." << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        
        try {
            std::string prompt = "Extract the " + std::to_string(max_keywords) + 
                               " most important keywords from the following text. "
                               "Return them as a numbered list with brief explanations of why each keyword is important.";
            nlohmann::json input = {{"text", text}};
            
            AnalysisResult result = engine_->analyze(prompt, input, "keyword_extraction", mode_);
            std::string keywords = result.content;
            
            std::cout << "🔑 Keywords:" << std::endl;
            std::cout << keywords << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "❌ Error during keyword extraction: " << e.what() << std::endl;
        }
    }
    
    void translateText(const std::string& text, const std::string& target_language = "English") {
        if (text.empty()) {
            std::cerr << "❌ No text provided for translation!" << std::endl;
            return;
        }
        
        std::cout << "\n🌍 Translating text to " << target_language << "..." << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        
        try {
            std::string prompt = "Translate the following text to " + target_language + 
                               ". Maintain the original meaning, tone, and style. "
                               "If the text is already in " + target_language + ", provide a polished version.";
            nlohmann::json input = {{"text", text}};
            
            AnalysisResult result = engine_->analyze(prompt, input, "translation", mode_);
            std::string translation = result.content;
            
            std::cout << "🌍 Translation:" << std::endl;
            std::cout << translation << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "❌ Error during translation: " << e.what() << std::endl;
        }
    }
    
    void analyzeSentiment(const std::string& text) {
        if (text.empty()) {
            std::cerr << "❌ No text provided for sentiment analysis!" << std::endl;
            return;
        }
        
        std::cout << "\n😊 Analyzing sentiment..." << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        
        try {
            std::string prompt = "Analyze the sentiment of the following text. Provide:\n"
                               "1. Overall sentiment (positive, negative, neutral)\n"
                               "2. Confidence level (0-100%)\n"
                               "3. Key emotional indicators\n"
                               "4. Brief explanation of your analysis";
            nlohmann::json input = {{"text", text}};
            
            AnalysisResult result = engine_->analyze(prompt, input, "sentiment_analysis", mode_);
            std::string sentiment = result.content;
            
            std::cout << "😊 Sentiment Analysis:" << std::endl;
            std::cout << sentiment << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "❌ Error during sentiment analysis: " << e.what() << std::endl;
        }
    }
    
    void generateQuestions(const std::string& text, int num_questions = 5) {
        if (text.empty()) {
            std::cerr << "❌ No text provided for question generation!" << std::endl;
            return;
        }
        
        std::cout << "\n❓ Generating questions about the text..." << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        
        try {
            std::string prompt = "Generate " + std::to_string(num_questions) + 
                               " thoughtful questions about the following text. "
                               "Include questions that test understanding, analysis, and critical thinking. "
                               "Make them specific and relevant to the content.";
            nlohmann::json input = {{"text", text}};
            
            AnalysisResult result = engine_->analyze(prompt, input, "question_generation", mode_);
            std::string questions = result.content;
            
            std::cout << "❓ Questions:" << std::endl;
            std::cout << questions << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "❌ Error during question generation: " << e.what() << std::endl;
        }
    }
    
    void processBatch(const std::string& input_dir, const std::string& operation) {
        if (!std::filesystem::exists(input_dir) || !std::filesystem::is_directory(input_dir)) {
            std::cerr << "❌ Directory not found: " << input_dir << std::endl;
            return;
        }
        
        std::vector<std::string> text_files = findTextFiles(input_dir);
        if (text_files.empty()) {
            std::cout << "📁 No text files found in " << input_dir << std::endl;
            return;
        }
        
        std::cout << "\n📁 Processing " << text_files.size() << " files in " << input_dir << std::endl;
        std::cout << "Operation: " << operation << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        for (const auto& file : text_files) {
            std::cout << "\n📄 Processing: " << file << std::endl;
            std::cout << std::string(40, '-') << std::endl;
            
            std::string content = readFile(file);
            if (!content.empty()) {
                std::string output_file = std::filesystem::path(file).stem().string() + 
                                        "_" + operation + ".txt";
                
                if (operation == "summarize") {
                    summarizeText(content, output_file);
                } else if (operation == "keywords") {
                    extractKeywords(content);
                } else if (operation == "sentiment") {
                    analyzeSentiment(content);
                } else if (operation == "questions") {
                    generateQuestions(content);
                } else {
                    std::cerr << "❌ Unknown operation: " << operation << std::endl;
                }
            }
        }
    }
    
private:
    std::string readFile(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return "";
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
    
    std::vector<std::string> findTextFiles(const std::string& dirpath) {
        std::vector<std::string> files;
        std::vector<std::string> extensions = {
            ".txt", ".md", ".rst", ".doc", ".docx", ".pdf",
            ".rtf", ".odt", ".tex", ".log", ".csv", ".json",
            ".xml", ".yaml", ".yml", ".ini", ".cfg", ".conf"
        };
        
        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(dirpath)) {
                if (entry.is_regular_file()) {
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    
                    if (std::find(extensions.begin(), extensions.end(), ext) != extensions.end()) {
                        files.push_back(entry.path().string());
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "❌ Error scanning directory: " << e.what() << std::endl;
        }
        
        return files;
    }
    
    void saveToFile(const std::string& filepath, const std::string& content, const std::string& type) {
        std::ofstream file(filepath);
        if (file.is_open()) {
            auto now = std::time(nullptr);
            auto tm = *std::localtime(&now);
            
            file << type << " Report\n";
            file << "==========\n";
            file << "Generated: " << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "\n";
            file << "Provider: " << engine_->getProviderName() << "\n";
            file << std::string(50, '=') << "\n\n";
            file << content;
            file.close();
            std::cout << "\n💾 " << type << " saved to: " << filepath << std::endl;
        } else {
            std::cerr << "❌ Failed to save " << type << " to file!" << std::endl;
        }
    }
};

void printUsage() {
    std::cout << "📝 LLMEngine Text Processor" << std::endl;
    std::cout << "===========================" << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  ./text_processor ollama <model> [mode] -s <text> [output_file]" << std::endl;
    std::cout << "  ./text_processor ollama <model> [mode] -k <text> [max_keywords]" << std::endl;
    std::cout << "  ./text_processor ollama <model> [mode] -t <text> <target_language> [source_language]" << std::endl;
    std::cout << "  ./text_processor ollama <model> [mode] -sa <text>" << std::endl;
    std::cout << "  ./text_processor ollama <model> [mode] -q <text> <num_questions>" << std::endl;
    std::cout << "  ./text_processor ollama <model> [mode] -b <directory> <operation>" << std::endl;
    std::cout << "  ./text_processor -s <text> [output_file]" << std::endl;
    std::cout << "  ./text_processor -f <file> [output_file]" << std::endl;
    std::cout << "  ./text_processor -k <text> [max_keywords]" << std::endl;
    std::cout << "  ./text_processor -t <text> [target_language]" << std::endl;
    std::cout << "  ./text_processor -a <text>" << std::endl;
    std::cout << "  ./text_processor -q <text> [num_questions]" << std::endl;
    std::cout << "  ./text_processor -b <directory> <operation>" << std::endl;
    std::cout << std::endl;
    std::cout << "Modes for Ollama:" << std::endl;
    std::cout << "  chat     - Conversational processing (default)" << std::endl;
    std::cout << "  generate - Text completion processing" << std::endl;
    std::cout << std::endl;
    std::cout << "Operations:" << std::endl;
    std::cout << "  -s, --summarize    Summarize text" << std::endl;
    std::cout << "  -f, --file         Summarize file" << std::endl;
    std::cout << "  -k, --keywords     Extract keywords" << std::endl;
    std::cout << "  -t, --translate    Translate text" << std::endl;
    std::cout << "  -sa, --sentiment   Analyze sentiment" << std::endl;
    std::cout << "  -q, --questions    Generate questions" << std::endl;
    std::cout << "  -b, --batch       Batch process directory" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  ./text_processor ollama qwen3:4b chat -s \"Long text here...\" summary.txt" << std::endl;
    std::cout << "  ./text_processor ollama qwen3:4b generate -k \"Text to analyze...\" 15" << std::endl;
    std::cout << "  ./text_processor ollama qwen3:4b -t \"Hello world\" Spanish" << std::endl;
    std::cout << "  ./text_processor ollama qwen3:4b -sa \"I love this product!\"" << std::endl;
    std::cout << "  ./text_processor ollama qwen3:4b -q \"Article content...\" 10" << std::endl;
    std::cout << "  ./text_processor ollama qwen3:4b -b ./documents summarize" << std::endl;
    std::cout << "  ./text_processor -s \"Long text here...\" summary.txt" << std::endl;
    std::cout << "  ./text_processor -f document.txt" << std::endl;
    std::cout << "  ./text_processor -k \"Text to analyze...\" 15" << std::endl;
    std::cout << "  ./text_processor -t \"Hello world\" Spanish" << std::endl;
    std::cout << "  ./text_processor -a \"I love this product!\"" << std::endl;
    std::cout << "  ./text_processor -q \"Article content...\" 10" << std::endl;
    std::cout << "  ./text_processor -b ./documents summarize" << std::endl;
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printUsage();
        return 1;
    }
    
    // Check if using Ollama first (no API key needed)
    if (argc > 1 && std::string(argv[1]) == "ollama") {
        try {
            std::string ollama_model = argc > 2 ? argv[2] : "qwen3:4b";
            std::string mode = argc > 3 ? argv[3] : "chat";
            
            if (mode != "chat" && mode != "generate") {
                std::cerr << "❌ Invalid mode. Use 'chat' or 'generate'" << std::endl;
                return 1;
            }
            
            TextProcessor processor("ollama", "", ollama_model, false, mode);
            
            // Handle remaining arguments for Ollama
            if (argc >= 5) {
                std::string operation = argv[4];
                
                if (operation == "-s" || operation == "--summarize") {
                    if (argc < 6) {
                        std::cerr << "❌ Text content required!" << std::endl;
                        return 1;
                    }
                    std::string text = argv[5];
                    std::string output_file = argc >= 7 ? argv[6] : "";
                    processor.summarizeText(text, output_file);
                }
                else if (operation == "-k" || operation == "--keywords") {
                    if (argc < 6) {
                        std::cerr << "❌ Text content required!" << std::endl;
                        return 1;
                    }
                    std::string text = argv[5];
                    processor.extractKeywords(text);
                }
                else if (operation == "-t" || operation == "--translate") {
                    if (argc < 7) {
                        std::cerr << "❌ Text and target language required!" << std::endl;
                        return 1;
                    }
                    std::string text = argv[5];
                    std::string target_lang = argv[6];
                    std::string source_lang = argc >= 8 ? argv[7] : "auto";
                    processor.translateText(text, target_lang);
                }
                else if (operation == "-sa" || operation == "--sentiment") {
                    if (argc < 6) {
                        std::cerr << "❌ Text content required!" << std::endl;
                        return 1;
                    }
                    std::string text = argv[5];
                    processor.analyzeSentiment(text);
                }
                else if (operation == "-q" || operation == "--questions") {
                    if (argc < 7) {
                        std::cerr << "❌ Text and number of questions required!" << std::endl;
                        return 1;
                    }
                    std::string text = argv[5];
                    int num_questions = std::stoi(argv[6]);
                    processor.generateQuestions(text, num_questions);
                }
                else if (operation == "-b" || operation == "--batch") {
                    if (argc < 7) {
                        std::cerr << "❌ Directory path and operation required!" << std::endl;
                        return 1;
                    }
                    std::string dirpath = argv[5];
                    std::string batch_operation = argv[6];
                    processor.processBatch(dirpath, batch_operation);
                }
                else {
                    std::cerr << "❌ Invalid operation!" << std::endl;
                    printUsage();
                    return 1;
                }
            } else {
                std::cerr << "❌ Please provide an operation to perform" << std::endl;
                printUsage();
                return 1;
            }
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "❌ Error: " << e.what() << std::endl;
            return 1;
        }
    }
    
    // Get API key from environment for online providers
    const char* api_key = std::getenv("QWEN_API_KEY");
    if (!api_key) {
        api_key = std::getenv("OPENAI_API_KEY");
    }
    if (!api_key) {
        api_key = std::getenv("ANTHROPIC_API_KEY");
    }
    
    if (!api_key) {
        std::cerr << "❌ No API key found! Please set one of:" << std::endl;
        std::cerr << "   export QWEN_API_KEY=\"your-key\"" << std::endl;
        std::cerr << "   export OPENAI_API_KEY=\"your-key\"" << std::endl;
        std::cerr << "   export ANTHROPIC_API_KEY=\"your-key\"" << std::endl;
        std::cerr << std::endl;
        std::cerr << "Or use Ollama (local) by running:" << std::endl;
        std::cerr << "   ./text_processor ollama <model> [mode] <operation> ..." << std::endl;
        return 1;
    }
    
    try {
        TextProcessor processor("qwen", api_key, "qwen-flash", false);
        
        std::string operation = argv[1];
        
        if (operation == "-s" || operation == "--summarize") {
            std::string text = argv[2];
            std::string output_file = argc >= 4 ? argv[3] : "";
            processor.summarizeText(text, output_file);
        }
        else if (operation == "-f" || operation == "--file") {
            std::string filepath = argv[2];
            std::string output_file = argc >= 4 ? argv[3] : "";
            processor.summarizeFile(filepath, output_file);
        }
        else if (operation == "-k" || operation == "--keywords") {
            std::string text = argv[2];
            int max_keywords = argc >= 4 ? std::stoi(argv[3]) : 10;
            processor.extractKeywords(text, max_keywords);
        }
        else if (operation == "-t" || operation == "--translate") {
            std::string text = argv[2];
            std::string target_language = argc >= 4 ? argv[3] : "English";
            processor.translateText(text, target_language);
        }
        else if (operation == "-a" || operation == "--analyze") {
            std::string text = argv[2];
            processor.analyzeSentiment(text);
        }
        else if (operation == "-q" || operation == "--questions") {
            std::string text = argv[2];
            int num_questions = argc >= 4 ? std::stoi(argv[3]) : 5;
            processor.generateQuestions(text, num_questions);
        }
        else if (operation == "-b" || operation == "--batch") {
            if (argc < 4) {
                std::cerr << "❌ Batch operation requires directory and operation type!" << std::endl;
                return 1;
            }
            std::string directory = argv[2];
            std::string batch_operation = argv[3];
            processor.processBatch(directory, batch_operation);
        }
        else {
            std::cerr << "❌ Unknown operation: " << operation << std::endl;
            printUsage();
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
