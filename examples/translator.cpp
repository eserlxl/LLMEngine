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
#include <map>

class Translator {
private:
    std::unique_ptr<LLMEngine> engine_;
    bool debug_mode_;
    std::string mode_;
    
    // Language mapping for better user experience
    std::map<std::string, std::string> language_map_;
    
public:
    Translator(const std::string& provider_name, const std::string& api_key, 
               const std::string& model = "", bool debug = false, const std::string& mode = "chat") 
        : debug_mode_(debug), mode_(mode) {
        try {
            engine_ = std::make_unique<LLMEngine>(provider_name, api_key, model, 
                                                 nlohmann::json{}, 24, debug);
            std::cout << "✓ Translator initialized with " << engine_->getProviderName() 
                      << " (" << (engine_->isOnlineProvider() ? "Online" : "Local") << ")" 
                      << " in " << mode_ << " mode" << std::endl;
            
            initializeLanguageMap();
        } catch (const std::exception& e) {
            std::cerr << "❌ Failed to initialize Translator: " << e.what() << std::endl;
            throw;
        }
    }
    
    void translateText(const std::string& text, const std::string& target_language, 
                       const std::string& source_language = "auto") {
        if (text.empty()) {
            std::cerr << "❌ No text provided for translation!" << std::endl;
            return;
        }
        
        std::cout << "\n🌍 Translating text..." << std::endl;
        std::cout << "Source: " << (source_language == "auto" ? "Auto-detect" : source_language) << std::endl;
        std::cout << "Target: " << target_language << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        
        try {
            std::string prompt = buildTranslationPrompt(source_language, target_language);
            nlohmann::json input = {{"text", text}};
            
            auto result = engine_->analyze(prompt, input, "translation", 0, mode_);
            std::string translation = result[1];
            
            std::cout << "🌍 Translation:" << std::endl;
            std::cout << translation << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "❌ Error during translation: " << e.what() << std::endl;
        }
    }
    
    void translateFile(const std::string& filepath, const std::string& target_language, 
                       const std::string& output_file = "") {
        if (!std::filesystem::exists(filepath)) {
            std::cerr << "❌ File not found: " << filepath << std::endl;
            return;
        }
        
        std::string content = readFile(filepath);
        if (content.empty()) {
            std::cerr << "❌ Failed to read file: " << filepath << std::endl;
            return;
        }
        
        std::cout << "\n📄 Translating file: " << filepath << std::endl;
        std::cout << "Target language: " << target_language << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        
        translateText(content, target_language);
        
        if (!output_file.empty()) {
            saveTranslation(content, target_language, output_file);
        }
    }
    
    void batchTranslate(const std::string& input_dir, const std::string& target_language, 
                        const std::string& output_dir = "") {
        if (!std::filesystem::exists(input_dir) || !std::filesystem::is_directory(input_dir)) {
            std::cerr << "❌ Directory not found: " << input_dir << std::endl;
            return;
        }
        
        std::vector<std::string> text_files = findTextFiles(input_dir);
        if (text_files.empty()) {
            std::cout << "📁 No text files found in " << input_dir << std::endl;
            return;
        }
        
        std::cout << "\n📁 Batch translating " << text_files.size() << " files..." << std::endl;
        std::cout << "Target language: " << target_language << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        for (const auto& file : text_files) {
            std::cout << "\n📄 Translating: " << file << std::endl;
            std::cout << std::string(40, '-') << std::endl;
            
            std::string content = readFile(file);
            if (!content.empty()) {
                std::string output_file = output_dir.empty() ? 
                    std::filesystem::path(file).stem().string() + "_" + target_language + ".txt" :
                    output_dir + "/" + std::filesystem::path(file).stem().string() + "_" + target_language + ".txt";
                
                translateText(content, target_language);
                saveTranslation(content, target_language, output_file);
            }
        }
    }
    
    void detectLanguage(const std::string& text) {
        if (text.empty()) {
            std::cerr << "❌ No text provided for language detection!" << std::endl;
            return;
        }
        
        std::cout << "\n🔍 Detecting language..." << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        
        try {
            std::string prompt = "Detect the language of the following text and provide:\n"
                               "1. The detected language name\n"
                               "2. Confidence level (0-100%)\n"
                               "3. Brief explanation of your detection";
            nlohmann::json input = {{"text", text}};
            
            auto result = engine_->analyze(prompt, input, "language_detection", 0, mode_);
            std::string detection = result[1];
            
            std::cout << "🔍 Language Detection:" << std::endl;
            std::cout << detection << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "❌ Error during language detection: " << e.what() << std::endl;
        }
    }
    
    void interactiveMode() {
        std::cout << "\n🌍 Interactive Translation Mode" << std::endl;
        std::cout << "Type 'quit' to exit, 'help' for commands" << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        
        std::string input;
        while (true) {
            std::cout << "\n📝 Enter text to translate: ";
            std::getline(std::cin, input);
            
            if (input.empty()) continue;
            
            if (input == "quit" || input == "exit") {
                std::cout << "👋 Goodbye!" << std::endl;
                break;
            }
            
            if (input == "help") {
                showHelp();
                continue;
            }
            
            std::cout << "🌍 Target language: ";
            std::string target_lang;
            std::getline(std::cin, target_lang);
            
            if (!target_lang.empty()) {
                translateText(input, target_lang);
            }
        }
    }
    
    void showSupportedLanguages() {
        std::cout << "\n🌍 Supported Languages:" << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        
        for (const auto& lang : language_map_) {
            std::cout << "  " << lang.first << " - " << lang.second << std::endl;
        }
        std::cout << std::endl;
    }
    
private:
    void initializeLanguageMap() {
        language_map_ = {
            {"en", "English"},
            {"es", "Spanish"},
            {"fr", "French"},
            {"de", "German"},
            {"it", "Italian"},
            {"pt", "Portuguese"},
            {"ru", "Russian"},
            {"ja", "Japanese"},
            {"ko", "Korean"},
            {"zh", "Chinese"},
            {"ar", "Arabic"},
            {"hi", "Hindi"},
            {"th", "Thai"},
            {"vi", "Vietnamese"},
            {"tr", "Turkish"},
            {"pl", "Polish"},
            {"nl", "Dutch"},
            {"sv", "Swedish"},
            {"da", "Danish"},
            {"no", "Norwegian"},
            {"fi", "Finnish"},
            {"cs", "Czech"},
            {"hu", "Hungarian"},
            {"ro", "Romanian"},
            {"bg", "Bulgarian"},
            {"hr", "Croatian"},
            {"sk", "Slovak"},
            {"sl", "Slovenian"},
            {"et", "Estonian"},
            {"lv", "Latvian"},
            {"lt", "Lithuanian"},
            {"el", "Greek"},
            {"he", "Hebrew"},
            {"fa", "Persian"},
            {"ur", "Urdu"},
            {"bn", "Bengali"},
            {"ta", "Tamil"},
            {"te", "Telugu"},
            {"ml", "Malayalam"},
            {"kn", "Kannada"},
            {"gu", "Gujarati"},
            {"pa", "Punjabi"},
            {"or", "Odia"},
            {"as", "Assamese"},
            {"ne", "Nepali"},
            {"si", "Sinhala"},
            {"my", "Burmese"},
            {"km", "Khmer"},
            {"lo", "Lao"},
            {"ka", "Georgian"},
            {"hy", "Armenian"},
            {"az", "Azerbaijani"},
            {"kk", "Kazakh"},
            {"ky", "Kyrgyz"},
            {"uz", "Uzbek"},
            {"tg", "Tajik"},
            {"mn", "Mongolian"},
            {"bo", "Tibetan"},
            {"dz", "Dzongkha"},
            {"sw", "Swahili"},
            {"am", "Amharic"},
            {"ti", "Tigrinya"},
            {"om", "Oromo"},
            {"so", "Somali"},
            {"ha", "Hausa"},
            {"yo", "Yoruba"},
            {"ig", "Igbo"},
            {"zu", "Zulu"},
            {"xh", "Xhosa"},
            {"af", "Afrikaans"},
            {"is", "Icelandic"},
            {"ga", "Irish"},
            {"cy", "Welsh"},
            {"eu", "Basque"},
            {"ca", "Catalan"},
            {"gl", "Galician"},
            {"mt", "Maltese"},
            {"mk", "Macedonian"},
            {"sq", "Albanian"},
            {"sr", "Serbian"},
            {"bs", "Bosnian"},
            {"me", "Montenegrin"},
            {"uk", "Ukrainian"},
            {"be", "Belarusian"},
            {"md", "Moldovan"},
            {"ro", "Romanian"},
            {"bg", "Bulgarian"},
            {"hr", "Croatian"},
            {"sk", "Slovak"},
            {"sl", "Slovenian"},
            {"et", "Estonian"},
            {"lv", "Latvian"},
            {"lt", "Lithuanian"},
            {"el", "Greek"},
            {"he", "Hebrew"},
            {"fa", "Persian"},
            {"ur", "Urdu"},
            {"bn", "Bengali"},
            {"ta", "Tamil"},
            {"te", "Telugu"},
            {"ml", "Malayalam"},
            {"kn", "Kannada"},
            {"gu", "Gujarati"},
            {"pa", "Punjabi"},
            {"or", "Odia"},
            {"as", "Assamese"},
            {"ne", "Nepali"},
            {"si", "Sinhala"},
            {"my", "Burmese"},
            {"km", "Khmer"},
            {"lo", "Lao"},
            {"ka", "Georgian"},
            {"hy", "Armenian"},
            {"az", "Azerbaijani"},
            {"kk", "Kazakh"},
            {"ky", "Kyrgyz"},
            {"uz", "Uzbek"},
            {"tg", "Tajik"},
            {"mn", "Mongolian"},
            {"bo", "Tibetan"},
            {"dz", "Dzongkha"},
            {"sw", "Swahili"},
            {"am", "Amharic"},
            {"ti", "Tigrinya"},
            {"om", "Oromo"},
            {"so", "Somali"},
            {"ha", "Hausa"},
            {"yo", "Yoruba"},
            {"ig", "Igbo"},
            {"zu", "Zulu"},
            {"xh", "Xhosa"},
            {"af", "Afrikaans"},
            {"is", "Icelandic"},
            {"ga", "Irish"},
            {"cy", "Welsh"},
            {"eu", "Basque"},
            {"ca", "Catalan"},
            {"gl", "Galician"},
            {"mt", "Maltese"},
            {"mk", "Macedonian"},
            {"sq", "Albanian"},
            {"sr", "Serbian"},
            {"bs", "Bosnian"},
            {"me", "Montenegrin"},
            {"uk", "Ukrainian"},
            {"be", "Belarusian"},
            {"md", "Moldovan"}
        };
    }
    
    std::string buildTranslationPrompt(const std::string& source_language, const std::string& target_language) {
        std::string prompt = "Translate the following text";
        
        if (source_language != "auto") {
            prompt += " from " + source_language;
        }
        
        prompt += " to " + target_language + ". ";
        prompt += "Maintain the original meaning, tone, and style. ";
        prompt += "If the text is already in " + target_language + ", provide a polished version. ";
        prompt += "Provide only the translation without additional explanations.";
        
        return prompt;
    }
    
    std::vector<std::string> findTextFiles(const std::string& dirpath) {
        std::vector<std::string> files;
        std::vector<std::string> extensions = {
            ".txt", ".md", ".rst", ".doc", ".docx", ".rtf", ".odt", ".tex"
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
    
    void saveTranslation(const std::string& original_text, const std::string& target_language, 
                        const std::string& output_file) {
        std::ofstream file(output_file);
        if (file.is_open()) {
            auto now = std::time(nullptr);
            auto tm = *std::localtime(&now);
            
            file << "Translation Report\n";
            file << "==================\n";
            file << "Target Language: " << target_language << "\n";
            file << "Generated: " << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "\n";
            file << "Provider: " << engine_->getProviderName() << "\n";
            file << std::string(50, '=') << "\n\n";
            file << "Original Text:\n";
            file << original_text << "\n\n";
            file << "Translation:\n";
            file << "[Translation will be added here]\n";
            file.close();
            std::cout << "\n💾 Translation template saved to: " << output_file << std::endl;
        } else {
            std::cerr << "❌ Failed to save translation!" << std::endl;
        }
    }
    
    void showHelp() {
        std::cout << "\n📋 Available Commands:" << std::endl;
        std::cout << "  help     - Show this help message" << std::endl;
        std::cout << "  languages - Show supported languages" << std::endl;
        std::cout << "  detect   - Detect language of text" << std::endl;
        std::cout << "  quit/exit - End interactive mode" << std::endl;
        std::cout << std::endl;
    }
    
    std::string readFile(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return "";
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
};

void printUsage() {
    std::cout << "🌍 LLMEngine Translator" << std::endl;
    std::cout << "=======================" << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  ./translator ollama <model> [mode] <text> [target_language] [source_language]" << std::endl;
    std::cout << "  ./translator ollama <model> [mode] -f <file> <target_language> [source_language]" << std::endl;
    std::cout << "  ./translator ollama <model> [mode] -d <directory> <target_language> [source_language]" << std::endl;
    std::cout << "  ./translator ollama <model> [mode] -i" << std::endl;
    std::cout << "  ./translator ollama <model> [mode] -l" << std::endl;
    std::cout << "  ./translator <text> <target_language>" << std::endl;
    std::cout << "  ./translator -f <file> <target_language> [output_file]" << std::endl;
    std::cout << "  ./translator -b <directory> <target_language> [output_dir]" << std::endl;
    std::cout << "  ./translator -d <text>" << std::endl;
    std::cout << "  ./translator -i" << std::endl;
    std::cout << "  ./translator -l" << std::endl;
    std::cout << std::endl;
    std::cout << "Modes for Ollama:" << std::endl;
    std::cout << "  chat     - Conversational translation (default)" << std::endl;
    std::cout << "  generate - Text completion translation" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -f, --file         Translate file" << std::endl;
    std::cout << "  -d, --directory    Translate directory" << std::endl;
    std::cout << "  -i, --interactive  Interactive mode" << std::endl;
    std::cout << "  -l, --languages    Show supported languages" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  ./translator ollama qwen3:4b chat \"Hello world\" Spanish" << std::endl;
    std::cout << "  ./translator ollama qwen3:4b generate -f document.txt French" << std::endl;
    std::cout << "  ./translator ollama qwen3:4b -d ./docs Spanish" << std::endl;
    std::cout << "  ./translator ollama qwen3:4b -i" << std::endl;
    std::cout << "  ./translator ollama qwen3:4b -l" << std::endl;
    std::cout << "  ./translator \"Hello world\" Spanish" << std::endl;
    std::cout << "  ./translator -f document.txt French" << std::endl;
    std::cout << "  ./translator -b ./texts/ German" << std::endl;
    std::cout << "  ./translator -d \"Bonjour le monde\"" << std::endl;
    std::cout << "  ./translator -i" << std::endl;
    std::cout << "  ./translator -l" << std::endl;
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
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
            
            Translator translator("ollama", "", ollama_model, false, mode);
            
            // Handle remaining arguments for Ollama
            if (argc >= 5) {
                std::string arg2 = argv[4];
                
                if (arg2 == "-f" || arg2 == "--file") {
                    if (argc < 7) {
                        std::cerr << "❌ File path and target language required!" << std::endl;
                        return 1;
                    }
                    std::string filepath = argv[5];
                    std::string target_lang = argv[6];
                    std::string source_lang = argc >= 8 ? argv[7] : "auto";
                    translator.translateFile(filepath, target_lang, source_lang);
                }
                else if (arg2 == "-d" || arg2 == "--directory") {
                    if (argc < 7) {
                        std::cerr << "❌ Directory path and target language required!" << std::endl;
                        return 1;
                    }
                    std::string dirpath = argv[5];
                    std::string target_lang = argv[6];
                    std::string source_lang = argc >= 8 ? argv[7] : "auto";
                    translator.batchTranslate(dirpath, target_lang, source_lang);
                }
                else if (arg2 == "-i" || arg2 == "--interactive") {
                    translator.interactiveMode();
                }
                else if (arg2 == "-l" || arg2 == "--languages") {
                    translator.showSupportedLanguages();
                }
                else {
                    // Single text translation
                    std::string text = argv[4];
                    std::string target_lang = argc >= 6 ? argv[5] : "English";
                    std::string source_lang = argc >= 7 ? argv[6] : "auto";
                    translator.translateText(text, target_lang, source_lang);
                }
            } else {
                std::cerr << "❌ Please provide text to translate or an operation" << std::endl;
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
        std::cerr << "   ./translator ollama <model> [mode] <text> [target_language]" << std::endl;
        return 1;
    }
    
    try {
        Translator translator("qwen", api_key, "qwen-flash", false);
        
        std::string arg1 = argv[1];
        
        if (arg1 == "-f" || arg1 == "--file") {
            if (argc < 4) {
                std::cerr << "❌ File path and target language required!" << std::endl;
                return 1;
            }
            std::string filepath = argv[2];
            std::string target_language = argv[3];
            std::string output_file = argc >= 5 ? argv[4] : "";
            translator.translateFile(filepath, target_language, output_file);
        }
        else if (arg1 == "-b" || arg1 == "--batch") {
            if (argc < 4) {
                std::cerr << "❌ Directory path and target language required!" << std::endl;
                return 1;
            }
            std::string directory = argv[2];
            std::string target_language = argv[3];
            std::string output_dir = argc >= 5 ? argv[4] : "";
            translator.batchTranslate(directory, target_language, output_dir);
        }
        else if (arg1 == "-d" || arg1 == "--detect") {
            if (argc < 3) {
                std::cerr << "❌ Text required for language detection!" << std::endl;
                return 1;
            }
            std::string text = argv[2];
            translator.detectLanguage(text);
        }
        else if (arg1 == "-i" || arg1 == "--interactive") {
            translator.interactiveMode();
        }
        else if (arg1 == "-l" || arg1 == "--languages") {
            translator.showSupportedLanguages();
        }
        else {
            // Direct translation
            if (argc < 3) {
                std::cerr << "❌ Target language required!" << std::endl;
                return 1;
            }
            std::string text = argv[1];
            std::string target_language = argv[2];
            translator.translateText(text, target_language);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
