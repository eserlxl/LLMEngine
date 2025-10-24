#include "LLMEngine.hpp"
#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <sstream>

class CodeAnalyzer {
private:
    std::unique_ptr<LLMEngine> engine_;
    bool debug_mode_;
    std::string mode_;
    
public:
    CodeAnalyzer(const std::string& provider_name, const std::string& api_key, 
                 const std::string& model = "", bool debug = false, const std::string& mode = "chat") 
        : debug_mode_(debug), mode_(mode) {
        try {
            // Use a more capable model for code analysis
            std::string analysis_model = model.empty() ? "qwen-max" : model;
            engine_ = std::make_unique<LLMEngine>(provider_name, api_key, analysis_model, 
                                                 nlohmann::json{}, 24, debug);
            std::cout << "✓ CodeAnalyzer initialized with " << engine_->getProviderName() 
                      << " (" << (engine_->isOnlineProvider() ? "Online" : "Local") << ")" 
                      << " in " << mode_ << " mode" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "❌ Failed to initialize CodeAnalyzer: " << e.what() << std::endl;
            throw;
        }
    }
    
    void analyzeFile(const std::string& filepath, const std::string& analysis_type = "comprehensive") {
        if (!std::filesystem::exists(filepath)) {
            std::cerr << "❌ File not found: " << filepath << std::endl;
            return;
        }
        
        std::string code = readFile(filepath);
        if (code.empty()) {
            std::cerr << "❌ Failed to read file: " << filepath << std::endl;
            return;
        }
        
        std::string language = detectLanguage(filepath);
        std::cout << "\n🔍 Analyzing " << filepath << " (" << language << ")" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        try {
            std::string prompt = buildAnalysisPrompt(analysis_type, language);
            nlohmann::json input = {
                {"code", code},
                {"language", language},
                {"filepath", filepath}
            };
            
            auto result = engine_->analyze(prompt, input, "code_analysis", 0, mode_);
            std::string analysis = result[1];
            
            std::cout << analysis << std::endl;
            
            // Save analysis to file
            saveAnalysis(filepath, analysis, analysis_type);
            
        } catch (const std::exception& e) {
            std::cerr << "❌ Error during analysis: " << e.what() << std::endl;
        }
    }
    
    void analyzeDirectory(const std::string& dirpath, const std::string& analysis_type = "comprehensive") {
        if (!std::filesystem::exists(dirpath) || !std::filesystem::is_directory(dirpath)) {
            std::cerr << "❌ Directory not found: " << dirpath << std::endl;
            return;
        }
        
        std::vector<std::string> code_files = findCodeFiles(dirpath);
        if (code_files.empty()) {
            std::cout << "📁 No code files found in " << dirpath << std::endl;
            return;
        }
        
        std::cout << "\n📁 Analyzing " << code_files.size() << " files in " << dirpath << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        for (const auto& file : code_files) {
            analyzeFile(file, analysis_type);
            std::cout << "\n" << std::string(60, '-') << std::endl;
        }
    }
    
    void compareFiles(const std::string& file1, const std::string& file2) {
        if (!std::filesystem::exists(file1) || !std::filesystem::exists(file2)) {
            std::cerr << "❌ One or both files not found!" << std::endl;
            return;
        }
        
        std::string code1 = readFile(file1);
        std::string code2 = readFile(file2);
        
        if (code1.empty() || code2.empty()) {
            std::cerr << "❌ Failed to read one or both files!" << std::endl;
            return;
        }
        
        std::cout << "\n🔄 Comparing files:" << std::endl;
        std::cout << "  File 1: " << file1 << std::endl;
        std::cout << "  File 2: " << file2 << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        try {
            std::string prompt = "Compare these two code files and provide a detailed analysis of differences, improvements, and recommendations:";
            nlohmann::json input = {
                {"code1", code1},
                {"code2", code2},
                {"file1", file1},
                {"file2", file2}
            };
            
            auto result = engine_->analyze(prompt, input, "code_comparison", 0, mode_);
            std::string comparison = result[1];
            
            std::cout << comparison << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "❌ Error during comparison: " << e.what() << std::endl;
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
    
    std::string detectLanguage(const std::string& filepath) {
        std::string ext = std::filesystem::path(filepath).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        
        if (ext == ".cpp" || ext == ".cc" || ext == ".cxx") return "C++";
        if (ext == ".c") return "C";
        if (ext == ".py") return "Python";
        if (ext == ".js") return "JavaScript";
        if (ext == ".ts") return "TypeScript";
        if (ext == ".java") return "Java";
        if (ext == ".go") return "Go";
        if (ext == ".rs") return "Rust";
        if (ext == ".php") return "PHP";
        if (ext == ".rb") return "Ruby";
        if (ext == ".swift") return "Swift";
        if (ext == ".kt") return "Kotlin";
        if (ext == ".scala") return "Scala";
        if (ext == ".hs") return "Haskell";
        if (ext == ".ml" || ext == ".mli") return "OCaml";
        if (ext == ".fs" || ext == ".fsx") return "F#";
        if (ext == ".cs") return "C#";
        if (ext == ".vb") return "VB.NET";
        if (ext == ".sh" || ext == ".bash") return "Bash";
        if (ext == ".ps1") return "PowerShell";
        if (ext == ".sql") return "SQL";
        if (ext == ".html" || ext == ".htm") return "HTML";
        if (ext == ".css") return "CSS";
        if (ext == ".scss" || ext == ".sass") return "SCSS/Sass";
        if (ext == ".less") return "Less";
        if (ext == ".xml") return "XML";
        if (ext == ".json") return "JSON";
        if (ext == ".yaml" || ext == ".yml") return "YAML";
        if (ext == ".toml") return "TOML";
        if (ext == ".ini") return "INI";
        if (ext == ".cfg" || ext == ".conf") return "Config";
        if (ext == ".cmake") return "CMake";
        if (ext == ".makefile" || ext == ".mk") return "Makefile";
        
        return "Unknown";
    }
    
    std::string buildAnalysisPrompt(const std::string& analysis_type, const std::string& language) {
        if (analysis_type == "security") {
            return "Perform a comprehensive security analysis of this " + language + " code. Look for vulnerabilities, security anti-patterns, input validation issues, authentication/authorization problems, and suggest security improvements.";
        }
        else if (analysis_type == "performance") {
            return "Analyze this " + language + " code for performance issues. Identify bottlenecks, inefficient algorithms, memory leaks, resource management problems, and suggest optimizations.";
        }
        else if (analysis_type == "style") {
            return "Review this " + language + " code for style and best practices. Check naming conventions, code organization, documentation, readability, and suggest improvements following " + language + " best practices.";
        }
        else if (analysis_type == "bugs") {
            return "Find bugs and potential issues in this " + language + " code. Look for logic errors, edge cases, null pointer dereferences, array bounds issues, and other common programming mistakes.";
        }
        else { // comprehensive
            return "Perform a comprehensive code review of this " + language + " code. Analyze:\n"
                   "1. Code quality and style\n"
                   "2. Potential bugs and issues\n"
                   "3. Security vulnerabilities\n"
                   "4. Performance considerations\n"
                   "5. Best practices adherence\n"
                   "6. Maintainability and readability\n"
                   "Provide specific recommendations for improvement.";
        }
    }
    
    std::vector<std::string> findCodeFiles(const std::string& dirpath) {
        std::vector<std::string> files;
        std::vector<std::string> extensions = {
            ".cpp", ".cc", ".cxx", ".c", ".hpp", ".h",
            ".py", ".js", ".ts", ".java", ".go", ".rs",
            ".php", ".rb", ".swift", ".kt", ".scala",
            ".hs", ".ml", ".mli", ".fs", ".fsx", ".cs",
            ".vb", ".sh", ".bash", ".ps1", ".sql"
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
    
    void saveAnalysis(const std::string& filepath, const std::string& analysis, const std::string& analysis_type) {
        std::string filename = std::filesystem::path(filepath).stem().string() + 
                              "_analysis_" + analysis_type + ".txt";
        
        std::ofstream file(filename);
        if (file.is_open()) {
            file << "Code Analysis Report\n";
            file << "===================\n";
            file << "File: " << filepath << "\n";
            file << "Analysis Type: " << analysis_type << "\n";
            file << "Provider: " << engine_->getProviderName() << "\n";
            file << std::string(50, '=') << "\n\n";
            file << analysis;
            file.close();
            std::cout << "\n💾 Analysis saved to: " << filename << std::endl;
        }
    }
};

void printUsage() {
    std::cout << "🔍 LLMEngine Code Analyzer" << std::endl;
    std::cout << "==========================" << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  ./code_analyzer ollama <model> [mode] <file> [analysis_type]" << std::endl;
    std::cout << "  ./code_analyzer ollama <model> [mode] -d <directory> [analysis_type]" << std::endl;
    std::cout << "  ./code_analyzer ollama <model> [mode] -c <file1> <file2>" << std::endl;
    std::cout << "  ./code_analyzer <file> [analysis_type]" << std::endl;
    std::cout << "  ./code_analyzer -d <directory> [analysis_type]" << std::endl;
    std::cout << "  ./code_analyzer -c <file1> <file2>" << std::endl;
    std::cout << std::endl;
    std::cout << "Modes for Ollama:" << std::endl;
    std::cout << "  chat     - Conversational analysis (default)" << std::endl;
    std::cout << "  generate - Text completion analysis" << std::endl;
    std::cout << std::endl;
    std::cout << "Analysis Types:" << std::endl;
    std::cout << "  comprehensive (default) - Full code review" << std::endl;
    std::cout << "  security               - Security analysis" << std::endl;
    std::cout << "  performance            - Performance analysis" << std::endl;
    std::cout << "  style                  - Style and best practices" << std::endl;
    std::cout << "  bugs                   - Bug detection" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  ./code_analyzer ollama qwen3:4b chat main.cpp" << std::endl;
    std::cout << "  ./code_analyzer ollama qwen3:4b generate main.cpp security" << std::endl;
    std::cout << "  ./code_analyzer ollama qwen3:4b -d src/" << std::endl;
    std::cout << "  ./code_analyzer ollama qwen3:4b -c old.cpp new.cpp" << std::endl;
    std::cout << "  ./code_analyzer main.cpp" << std::endl;
    std::cout << "  ./code_analyzer main.cpp security" << std::endl;
    std::cout << "  ./code_analyzer -d src/" << std::endl;
    std::cout << "  ./code_analyzer -c old.cpp new.cpp" << std::endl;
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
            
            CodeAnalyzer analyzer("ollama", "", ollama_model, false, mode);
            
            // Handle remaining arguments for Ollama
            if (argc >= 4) {
                std::string arg2 = argv[4];
                if (arg2 == "-d" && argc >= 6) {
                    // Directory analysis
                    std::string dirpath = argv[5];
                    std::string analysis_type = argc >= 7 ? argv[6] : "comprehensive";
                    analyzer.analyzeDirectory(dirpath, analysis_type);
                }
                else if (arg2 == "-c" && argc >= 7) {
                    // File comparison
                    std::string file1 = argv[5];
                    std::string file2 = argv[6];
                    analyzer.compareFiles(file1, file2);
                }
                else {
                    // Single file analysis
                    std::string filepath = argv[4];
                    std::string analysis_type = argc >= 6 ? argv[5] : "comprehensive";
                    analyzer.analyzeFile(filepath, analysis_type);
                }
            } else {
                std::cerr << "❌ Please provide a file or directory to analyze" << std::endl;
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
        std::cerr << "   ./code_analyzer ollama <model> [mode] <file>" << std::endl;
        return 1;
    }
    
    try {
        CodeAnalyzer analyzer("qwen", api_key, "qwen-max", false);
        
        std::string arg1 = argv[1];
        
        if (arg1 == "-d" && argc >= 3) {
            // Directory analysis
            std::string dirpath = argv[2];
            std::string analysis_type = argc >= 4 ? argv[3] : "comprehensive";
            analyzer.analyzeDirectory(dirpath, analysis_type);
        }
        else if (arg1 == "-c" && argc >= 4) {
            // File comparison
            std::string file1 = argv[2];
            std::string file2 = argv[3];
            analyzer.compareFiles(file1, file2);
        }
        else {
            // Single file analysis
            std::string filepath = argv[1];
            std::string analysis_type = argc >= 3 ? argv[2] : "comprehensive";
            analyzer.analyzeFile(filepath, analysis_type);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
