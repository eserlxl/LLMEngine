// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This example provides a file analysis utility built with LLMEngine and is
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
#include <map>

using namespace LLMEngine;

class FileAnalyzer {
private:
    std::unique_ptr<LLMEngine> engine_;
    bool debug_mode_;
    std::string mode_;
    
public:
    FileAnalyzer(const std::string& provider_name, const std::string& api_key, 
                 const std::string& model = "", bool debug = false, const std::string& mode = "chat") 
        : debug_mode_(debug), mode_(mode) {
        try {
            // Configure parameters optimized for file analysis
            nlohmann::json file_params = {
                {"temperature", 0.4},        // Balanced for file analysis
                {"max_tokens", 3500},         // Good length for file analysis
                {"top_p", 0.85},             // Focused sampling for analysis
                {"frequency_penalty", 0.0},   // No penalty for technical terms
                {"presence_penalty", 0.0}     // No penalty for introducing concepts
            };
            
            engine_ = std::make_unique<LLMEngine>(provider_name, api_key, model, 
                                                 file_params, 24, debug);
            std::cout << "✓ FileAnalyzer initialized with " << engine_->getProviderName() 
                      << " (" << (engine_->isOnlineProvider() ? "Online" : "Local") << ")" 
                      << " in " << mode_ << " mode" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "❌ Failed to initialize FileAnalyzer: " << e.what() << std::endl;
            throw;
        }
    }
    
    void analyzeFile(const std::string& filepath) {
        if (!std::filesystem::exists(filepath)) {
            std::cerr << "❌ File not found: " << filepath << std::endl;
            return;
        }
        
        auto file_info = getFileInfo(filepath);
        std::string content = readFile(filepath);
        
        std::cout << "\n📁 Analyzing file: " << filepath << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        // Display basic file information
        displayFileInfo(file_info);
        
        if (content.empty()) {
            std::cerr << "❌ Failed to read file content!" << std::endl;
            return;
        }
        
        // Analyze content based on file type
        std::string file_type = detectFileType(filepath);
        analyzeContent(content, file_type, filepath);
    }
    
    void analyzeDirectory(const std::string& dirpath) {
        if (!std::filesystem::exists(dirpath) || !std::filesystem::is_directory(dirpath)) {
            std::cerr << "❌ Directory not found: " << dirpath << std::endl;
            return;
        }
        
        std::cout << "\n📁 Analyzing directory: " << dirpath << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        auto stats = getDirectoryStats(dirpath);
        displayDirectoryStats(stats);
        
        // Analyze largest files
        auto large_files = getLargestFiles(dirpath, 5);
        if (!large_files.empty()) {
            std::cout << "\n📊 Largest files:" << std::endl;
            for (const auto& file : large_files) {
                std::cout << "  " << file.first << " (" << formatFileSize(file.second) << ")" << std::endl;
            }
        }
        
        // Analyze file types
        auto file_types = getFileTypeDistribution(dirpath);
        if (!file_types.empty()) {
            std::cout << "\n📈 File type distribution:" << std::endl;
            for (const auto& type : file_types) {
                std::cout << "  " << type.first << ": " << type.second << " files" << std::endl;
            }
        }
    }
    
    void findDuplicates(const std::string& dirpath) {
        if (!std::filesystem::exists(dirpath) || !std::filesystem::is_directory(dirpath)) {
            std::cerr << "❌ Directory not found: " << dirpath << std::endl;
            return;
        }
        
        std::cout << "\n🔍 Searching for duplicate files in: " << dirpath << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        auto duplicates = findDuplicateFiles(dirpath);
        
        if (duplicates.empty()) {
            std::cout << "✅ No duplicate files found!" << std::endl;
            return;
        }
        
        std::cout << "🔍 Found " << duplicates.size() << " groups of duplicate files:" << std::endl;
        for (size_t i = 0; i < duplicates.size(); ++i) {
            std::cout << "\nGroup " << (i + 1) << ":" << std::endl;
            for (const auto& file : duplicates[i]) {
                std::cout << "  " << file << std::endl;
            }
        }
    }
    
    void generateReport(const std::string& path, const std::string& output_file = "") {
        std::string report_content;
        
        if (std::filesystem::is_directory(path)) {
            report_content = generateDirectoryReport(path);
        } else {
            report_content = generateFileReport(path);
        }
        
        std::string filename = output_file.empty() ? 
            "file_analysis_report_" + getTimestamp() + ".txt" : output_file;
        
        saveReport(filename, report_content, path);
    }
    
private:
    struct FileInfo {
        std::string name;
        std::string extension;
        std::string type;
        size_t size;
        std::string last_modified;
        std::string permissions;
    };
    
    struct DirectoryStats {
        size_t total_files;
        size_t total_directories;
        size_t total_size;
        std::string oldest_file;
        std::string newest_file;
    };
    
    FileInfo getFileInfo(const std::string& filepath) {
        FileInfo info;
        std::filesystem::path path(filepath);
        
        info.name = path.filename().string();
        info.extension = path.extension().string();
        info.type = detectFileType(filepath);
        info.size = std::filesystem::file_size(path);
        
        auto ftime = std::filesystem::last_write_time(path);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
        auto time_t = std::chrono::system_clock::to_time_t(sctp);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        info.last_modified = ss.str();
        
        auto perms = std::filesystem::status(path).permissions();
        info.permissions = formatPermissions(perms);
        
        return info;
    }
    
    DirectoryStats getDirectoryStats(const std::string& dirpath) {
        DirectoryStats stats = {0, 0, 0, "", ""};
        std::time_t oldest_time = std::time(nullptr);
        std::time_t newest_time = 0;
        
        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(dirpath)) {
                if (entry.is_regular_file()) {
                    stats.total_files++;
                    stats.total_size += entry.file_size();
                    
                    auto ftime = std::filesystem::last_write_time(entry);
                    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                        ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
                    auto time_t = std::chrono::system_clock::to_time_t(sctp);
                    
                    if (time_t < oldest_time) {
                        oldest_time = time_t;
                        stats.oldest_file = entry.path().string();
                    }
                    if (time_t > newest_time) {
                        newest_time = time_t;
                        stats.newest_file = entry.path().string();
                    }
                } else if (entry.is_directory()) {
                    stats.total_directories++;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "❌ Error scanning directory: " << e.what() << std::endl;
        }
        
        return stats;
    }
    
    std::vector<std::pair<std::string, size_t>> getLargestFiles(const std::string& dirpath, int count) {
        std::vector<std::pair<std::string, size_t>> files;
        
        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(dirpath)) {
                if (entry.is_regular_file()) {
                    files.push_back({entry.path().string(), entry.file_size()});
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "❌ Error scanning directory: " << e.what() << std::endl;
        }
        
        std::sort(files.begin(), files.end(), 
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        
        if (files.size() > count) {
            files.resize(count);
        }
        
        return files;
    }
    
    std::map<std::string, int> getFileTypeDistribution(const std::string& dirpath) {
        std::map<std::string, int> types;
        
        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(dirpath)) {
                if (entry.is_regular_file()) {
                    std::string ext = entry.path().extension().string();
                    if (ext.empty()) ext = "no extension";
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    types[ext]++;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "❌ Error scanning directory: " << e.what() << std::endl;
        }
        
        return types;
    }
    
    std::vector<std::vector<std::string>> findDuplicateFiles(const std::string& dirpath) {
        std::map<std::string, std::vector<std::string>> file_hashes;
        
        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(dirpath)) {
                if (entry.is_regular_file()) {
                    std::string hash = calculateFileHash(entry.path().string());
                    file_hashes[hash].push_back(entry.path().string());
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "❌ Error scanning directory: " << e.what() << std::endl;
        }
        
        std::vector<std::vector<std::string>> duplicates;
        for (const auto& pair : file_hashes) {
            if (pair.second.size() > 1) {
                duplicates.push_back(pair.second);
            }
        }
        
        return duplicates;
    }
    
    std::string calculateFileHash(const std::string& filepath) {
        std::ifstream file(filepath, std::ios::binary);
        if (!file) return "";
        
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        
        // Simple hash function (for demo purposes)
        size_t hash = 0;
        for (char c : content) {
            hash = hash * 31 + c;
        }
        
        return std::to_string(hash);
    }
    
    std::string detectFileType(const std::string& filepath) {
        std::string ext = std::filesystem::path(filepath).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        
        if (ext == ".txt" || ext == ".md" || ext == ".rst") return "Text Document";
        if (ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".hpp" || ext == ".h") return "C/C++ Source";
        if (ext == ".py") return "Python Script";
        if (ext == ".js" || ext == ".ts") return "JavaScript/TypeScript";
        if (ext == ".java") return "Java Source";
        if (ext == ".go") return "Go Source";
        if (ext == ".rs") return "Rust Source";
        if (ext == ".php") return "PHP Script";
        if (ext == ".rb") return "Ruby Script";
        if (ext == ".swift") return "Swift Source";
        if (ext == ".kt") return "Kotlin Source";
        if (ext == ".scala") return "Scala Source";
        if (ext == ".hs") return "Haskell Source";
        if (ext == ".ml" || ext == ".mli") return "OCaml Source";
        if (ext == ".fs" || ext == ".fsx") return "F# Source";
        if (ext == ".cs") return "C# Source";
        if (ext == ".vb") return "VB.NET Source";
        if (ext == ".sh" || ext == ".bash") return "Shell Script";
        if (ext == ".ps1") return "PowerShell Script";
        if (ext == ".sql") return "SQL Script";
        if (ext == ".html" || ext == ".htm") return "HTML Document";
        if (ext == ".css") return "CSS Stylesheet";
        if (ext == ".scss" || ext == ".sass") return "SCSS/Sass Stylesheet";
        if (ext == ".less") return "Less Stylesheet";
        if (ext == ".xml") return "XML Document";
        if (ext == ".json") return "JSON Data";
        if (ext == ".yaml" || ext == ".yml") return "YAML Data";
        if (ext == ".toml") return "TOML Data";
        if (ext == ".ini") return "INI Configuration";
        if (ext == ".cfg" || ext == ".conf") return "Configuration File";
        if (ext == ".cmake") return "CMake Script";
        if (ext == ".makefile" || ext == ".mk") return "Makefile";
        if (ext == ".pdf") return "PDF Document";
        if (ext == ".doc" || ext == ".docx") return "Word Document";
        if (ext == ".xls" || ext == ".xlsx") return "Excel Spreadsheet";
        if (ext == ".ppt" || ext == ".pptx") return "PowerPoint Presentation";
        if (ext == ".jpg" || ext == ".jpeg") return "JPEG Image";
        if (ext == ".png") return "PNG Image";
        if (ext == ".gif") return "GIF Image";
        if (ext == ".bmp") return "BMP Image";
        if (ext == ".svg") return "SVG Image";
        if (ext == ".mp4") return "MP4 Video";
        if (ext == ".avi") return "AVI Video";
        if (ext == ".mov") return "MOV Video";
        if (ext == ".wmv") return "WMV Video";
        if (ext == ".mp3") return "MP3 Audio";
        if (ext == ".wav") return "WAV Audio";
        if (ext == ".flac") return "FLAC Audio";
        if (ext == ".ogg") return "OGG Audio";
        if (ext == ".zip") return "ZIP Archive";
        if (ext == ".rar") return "RAR Archive";
        if (ext == ".7z") return "7-Zip Archive";
        if (ext == ".tar" || ext == ".gz" || ext == ".bz2") return "TAR Archive";
        
        return "Unknown";
    }
    
    void analyzeContent(const std::string& content, const std::string& file_type, const std::string& filepath) {
        try {
            std::string prompt = "Analyze this " + file_type + " file and provide insights about:\n"
                               "1. Content overview and purpose\n"
                               "2. Key characteristics and patterns\n"
                               "3. Potential issues or improvements\n"
                               "4. File organization and structure\n"
                               "5. Recommendations for optimization";
            
            nlohmann::json input = {
                {"content", content},
                {"file_type", file_type},
                {"filepath", filepath}
            };
            
            AnalysisResult result = engine_->analyze(prompt, input, "file_analysis", mode_);
            std::string analysis = result.content;
            
            std::cout << "\n📋 Content Analysis:" << std::endl;
            std::cout << analysis << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "❌ Error during content analysis: " << e.what() << std::endl;
        }
    }
    
    void displayFileInfo(const FileInfo& info) {
        std::cout << "📄 File Information:" << std::endl;
        std::cout << "  Name: " << info.name << std::endl;
        std::cout << "  Type: " << info.type << std::endl;
        std::cout << "  Size: " << formatFileSize(info.size) << std::endl;
        std::cout << "  Last Modified: " << info.last_modified << std::endl;
        std::cout << "  Permissions: " << info.permissions << std::endl;
    }
    
    void displayDirectoryStats(const DirectoryStats& stats) {
        std::cout << "📊 Directory Statistics:" << std::endl;
        std::cout << "  Total Files: " << stats.total_files << std::endl;
        std::cout << "  Total Directories: " << stats.total_directories << std::endl;
        std::cout << "  Total Size: " << formatFileSize(stats.total_size) << std::endl;
        if (!stats.oldest_file.empty()) {
            std::cout << "  Oldest File: " << stats.oldest_file << std::endl;
        }
        if (!stats.newest_file.empty()) {
            std::cout << "  Newest File: " << stats.newest_file << std::endl;
        }
    }
    
    std::string formatFileSize(size_t size) {
        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        int unit = 0;
        double fsize = static_cast<double>(size);
        
        while (fsize >= 1024.0 && unit < 4) {
            fsize /= 1024.0;
            unit++;
        }
        
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << fsize << " " << units[unit];
        return ss.str();
    }
    
    std::string formatPermissions(std::filesystem::perms perms) {
        std::string result = "";
        result += (perms & std::filesystem::perms::owner_read) != std::filesystem::perms::none ? "r" : "-";
        result += (perms & std::filesystem::perms::owner_write) != std::filesystem::perms::none ? "w" : "-";
        result += (perms & std::filesystem::perms::owner_exec) != std::filesystem::perms::none ? "x" : "-";
        result += (perms & std::filesystem::perms::group_read) != std::filesystem::perms::none ? "r" : "-";
        result += (perms & std::filesystem::perms::group_write) != std::filesystem::perms::none ? "w" : "-";
        result += (perms & std::filesystem::perms::group_exec) != std::filesystem::perms::none ? "x" : "-";
        result += (perms & std::filesystem::perms::others_read) != std::filesystem::perms::none ? "r" : "-";
        result += (perms & std::filesystem::perms::others_write) != std::filesystem::perms::none ? "w" : "-";
        result += (perms & std::filesystem::perms::others_exec) != std::filesystem::perms::none ? "x" : "-";
        return result;
    }
    
    std::string getTimestamp() {
        auto now = std::time(nullptr);
        auto tm = *std::localtime(&now);
        std::stringstream ss;
        ss << std::put_time(&tm, "%Y%m%d_%H%M%S");
        return ss.str();
    }
    
    std::string generateFileReport(const std::string& filepath) {
        auto info = getFileInfo(filepath);
        std::string content = readFile(filepath);
        
        std::stringstream report;
        report << "File Analysis Report\n";
        report << "===================\n";
        report << "File: " << filepath << "\n";
        report << "Generated: " << getTimestamp() << "\n";
        report << std::string(50, '=') << "\n\n";
        
        report << "Basic Information:\n";
        report << "- Name: " << info.name << "\n";
        report << "- Type: " << info.type << "\n";
        report << "- Size: " << formatFileSize(info.size) << "\n";
        report << "- Last Modified: " << info.last_modified << "\n";
        report << "- Permissions: " << info.permissions << "\n\n";
        
        if (!content.empty()) {
            report << "Content Analysis:\n";
            report << "- Character Count: " << content.length() << "\n";
            report << "- Line Count: " << std::count(content.begin(), content.end(), '\n') + 1 << "\n";
            report << "- Word Count: " << std::count(content.begin(), content.end(), ' ') + 1 << "\n";
        }
        
        return report.str();
    }
    
    std::string generateDirectoryReport(const std::string& dirpath) {
        auto stats = getDirectoryStats(dirpath);
        auto file_types = getFileTypeDistribution(dirpath);
        auto large_files = getLargestFiles(dirpath, 10);
        
        std::stringstream report;
        report << "Directory Analysis Report\n";
        report << "========================\n";
        report << "Directory: " << dirpath << "\n";
        report << "Generated: " << getTimestamp() << "\n";
        report << std::string(50, '=') << "\n\n";
        
        report << "Statistics:\n";
        report << "- Total Files: " << stats.total_files << "\n";
        report << "- Total Directories: " << stats.total_directories << "\n";
        report << "- Total Size: " << formatFileSize(stats.total_size) << "\n\n";
        
        report << "File Type Distribution:\n";
        for (const auto& type : file_types) {
            report << "- " << type.first << ": " << type.second << " files\n";
        }
        report << "\n";
        
        report << "Largest Files:\n";
        for (const auto& file : large_files) {
            report << "- " << file.first << " (" << formatFileSize(file.second) << ")\n";
        }
        
        return report.str();
    }
    
    void saveReport(const std::string& filename, const std::string& content, const std::string& path) {
        std::ofstream file(filename);
        if (file.is_open()) {
            file << content;
            file.close();
            std::cout << "\n💾 Report saved to: " << filename << std::endl;
        } else {
            std::cerr << "❌ Failed to save report!" << std::endl;
        }
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
    std::cout << "📁 LLMEngine File Analyzer" << std::endl;
    std::cout << "==========================" << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  ./file_analyzer ollama <model> [mode] <file>" << std::endl;
    std::cout << "  ./file_analyzer ollama <model> [mode] -d <directory>" << std::endl;
    std::cout << "  ./file_analyzer ollama <model> [mode] -duplicates <directory>" << std::endl;
    std::cout << "  ./file_analyzer ollama <model> [mode] -report <path> [output_file]" << std::endl;
    std::cout << "  ./file_analyzer <file>" << std::endl;
    std::cout << "  ./file_analyzer -d <directory>" << std::endl;
    std::cout << "  ./file_analyzer -duplicates <directory>" << std::endl;
    std::cout << "  ./file_analyzer -report <path> [output_file]" << std::endl;
    std::cout << std::endl;
    std::cout << "Modes for Ollama:" << std::endl;
    std::cout << "  chat     - Conversational analysis (default)" << std::endl;
    std::cout << "  generate - Text completion analysis" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -d, --directory    Analyze directory" << std::endl;
    std::cout << "  -duplicates        Find duplicate files" << std::endl;
    std::cout << "  -report            Generate detailed report" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  ./file_analyzer ollama qwen3:4b chat document.txt" << std::endl;
    std::cout << "  ./file_analyzer ollama qwen3:4b generate -d ./src/" << std::endl;
    std::cout << "  ./file_analyzer ollama qwen3:4b -duplicates ./downloads/" << std::endl;
    std::cout << "  ./file_analyzer ollama qwen3:4b -report ./project/ report.txt" << std::endl;
    std::cout << "  ./file_analyzer document.txt" << std::endl;
    std::cout << "  ./file_analyzer -d ./src/" << std::endl;
    std::cout << "  ./file_analyzer -duplicates ./downloads/" << std::endl;
    std::cout << "  ./file_analyzer -report ./project/ report.txt" << std::endl;
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
            
            FileAnalyzer analyzer("ollama", "", ollama_model, false, mode);
            
            // Handle remaining arguments for Ollama
            if (argc >= 4) {
                std::string arg2 = argv[4];
                
                if (arg2 == "-d" || arg2 == "--directory") {
                    if (argc < 6) {
                        std::cerr << "❌ Directory path required!" << std::endl;
                        return 1;
                    }
                    std::string dirpath = argv[5];
                    analyzer.analyzeDirectory(dirpath);
                }
                else if (arg2 == "-duplicates") {
                    if (argc < 6) {
                        std::cerr << "❌ Directory path required!" << std::endl;
                        return 1;
                    }
                    std::string dirpath = argv[5];
                    analyzer.findDuplicates(dirpath);
                }
                else if (arg2 == "-report") {
                    if (argc < 7) {
                        std::cerr << "❌ Directory path and output file required!" << std::endl;
                        return 1;
                    }
                    std::string dirpath = argv[5];
                    std::string output_file = argv[6];
                    analyzer.generateReport(dirpath, output_file);
                }
                else {
                    // Single file analysis
                    std::string filepath = argv[4];
                    analyzer.analyzeFile(filepath);
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
        std::cerr << "   ./file_analyzer ollama <model> [mode] <file>" << std::endl;
        return 1;
    }
    
    try {
        FileAnalyzer analyzer("qwen", api_key, "qwen-flash", false);
        
        std::string arg1 = argv[1];
        
        if (arg1 == "-d" || arg1 == "--directory") {
            if (argc < 3) {
                std::cerr << "❌ Directory path required!" << std::endl;
                return 1;
            }
            std::string dirpath = argv[2];
            analyzer.analyzeDirectory(dirpath);
        }
        else if (arg1 == "-duplicates") {
            if (argc < 3) {
                std::cerr << "❌ Directory path required!" << std::endl;
                return 1;
            }
            std::string dirpath = argv[2];
            analyzer.findDuplicates(dirpath);
        }
        else if (arg1 == "-report") {
            if (argc < 3) {
                std::cerr << "❌ Path required!" << std::endl;
                return 1;
            }
            std::string path = argv[2];
            std::string output_file = argc >= 4 ? argv[3] : "";
            analyzer.generateReport(path, output_file);
        }
        else {
            // Single file analysis
            std::string filepath = argv[1];
            analyzer.analyzeFile(filepath);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
