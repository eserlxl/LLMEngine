// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This example demonstrates AI-powered logo generation with LLMEngine and is
// licensed under the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine.hpp"

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace LLMEngine;

class LogoGenerator {
  private:
    std::unique_ptr<LLMEngine::LLMEngine> engine_;
    bool debug_mode_;
    std::string output_dir_;

  public:
    LogoGenerator(const std::string& provider_name,
                  const std::string& api_key,
                  const std::string& model = "",
                  bool debug = false)
        : debug_mode_(debug) {
        try {
            // Configure parameters optimized for creative logo generation
            nlohmann::json logo_params = {
                {"temperature", 0.8},       // Higher creativity for logo design
                {"max_tokens", 32768},      // More tokens for detailed SVG code
                {"top_p", 0.9},             // Good balance of creativity and coherence
                {"frequency_penalty", 0.1}, // Slight penalty to avoid repetition
                {"presence_penalty", 0.0}   // No penalty for introducing new concepts
            };

            engine_ = std::make_unique<LLMEngine::LLMEngine>(
                provider_name, api_key, model, logo_params, 24, debug);
            output_dir_ = "generated_logos";
            std::filesystem::create_directories(output_dir_);

            std::cout << "✓ LogoGenerator initialized with " << engine_->getProviderName() << " ("
                      << (engine_->isOnlineProvider() ? "Online" : "Local") << ")" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "❌ Failed to initialize LogoGenerator: " << e.what() << std::endl;
            throw;
        }
    }

    void generateLogo(const std::string& description,
                      const std::string& format = "png",
                      const std::string& filename = "") {
        std::cout << "\n🎨 Generating logo from description: \"" << description << "\""
                  << std::endl;

        try {
            // Create detailed prompt for logo generation
            std::string prompt = createLogoPrompt(description, format);

            // Generate SVG logo code directly from LLM
            AnalysisResult result = engine_->analyze(prompt, nlohmann::json{}, "logo_generation");
            std::string svg_content = result.content;

            // Clean up the SVG content (remove any markdown formatting or extra text)
            svg_content = cleanSVGContent(svg_content);

            std::cout << "📋 Generated SVG logo code:" << std::endl;
            std::cout << svg_content << std::endl;

            // Validate SVG content
            if (!isValidSVG(svg_content)) {
                std::cerr << "⚠️  Warning: Generated SVG content may not be valid" << std::endl;
            }

            // Generate filename if not provided
            std::string output_filename =
                filename.empty() ? generateFilename(description, format) : filename;
            std::string output_path = output_dir_ + "/" + output_filename;

            // Save SVG file
            std::string svg_path = output_path.substr(0, output_path.find_last_of('.')) + ".svg";
            std::ofstream svg_file(svg_path);
            if (svg_file.is_open()) {
                svg_file << svg_content;
                svg_file.close();
                std::cout << "✓ SVG logo saved to: " << svg_path << std::endl;
            }

            // Convert to requested format
            if (format == "png" || format == "jpeg") {
                convertToRaster(svg_path, output_path, format);
            }

            std::cout << "🎉 Logo generation completed!" << std::endl;
            std::cout << "📁 Output directory: " << output_dir_ << std::endl;

        } catch (const std::exception& e) {
            std::cerr << "❌ Error generating logo: " << e.what() << std::endl;
        }
    }

    void generateMultipleLogos(const std::vector<std::string>& descriptions,
                               const std::string& format = "png") {
        std::cout << "\n🎨 Generating " << descriptions.size() << " logos..." << std::endl;

        for (size_t i = 0; i < descriptions.size(); ++i) {
            std::cout << "\n--- Logo " << (i + 1) << "/" << descriptions.size() << " ---"
                      << std::endl;
            generateLogo(descriptions[i], format);
        }

        std::cout << "\n🎉 All logos generated successfully!" << std::endl;
    }

    void interactiveMode() {
        std::cout << "\n🎨 Interactive Logo Generator" << std::endl;
        std::cout << "=============================" << std::endl;
        std::cout << "Type 'quit', 'exit', or 'bye' to end the session." << std::endl;
        std::cout << "Type 'help' for available commands." << std::endl;
        std::cout << std::string(50, '=') << std::endl;

        std::string user_input;
        while (true) {
            std::cout << "\n🎨 Describe your logo: ";
            std::cout.flush();

            if (!std::getline(std::cin, user_input)) {
                std::cout << "\n👋 Goodbye! Thanks for using LogoGenerator!" << std::endl;
                break;
            }

            if (user_input.empty())
                continue;

            // Handle special commands
            if (handleCommand(user_input)) {
                continue;
            }

            // Generate logo
            generateLogo(user_input);
        }
    }

  private:
    std::string createLogoPrompt(const std::string& description, const std::string& format) {
        std::stringstream prompt;
        prompt << "Create a complete SVG logo for: \"" << description << "\"\n\n";
        prompt << "Please generate a complete, valid SVG code that includes:\n";
        prompt << "1. Proper SVG header with appropriate width and height dimensions\n";
        prompt << "2. Professional design elements (shapes, text, gradients, etc.)\n";
        prompt << "3. Appropriate colors using hex codes\n";
        prompt << "4. Clean, modern styling suitable for business use\n";
        prompt << "5. Text elements if needed (use appropriate fonts)\n\n";
        prompt << "Return ONLY the complete SVG code, starting with <?xml version=\"1.0\" "
                  "encoding=\"UTF-8\"?> and ending with </svg>\n";
        prompt << "Do not include any explanations or additional text - just the SVG code.\n";
        prompt << "Make it professional, scalable, and visually appealing.";

        return prompt.str();
    }

    std::string generateFilename(const std::string& description, const std::string& format) {
        // Extract key words from description
        std::stringstream ss(description);
        std::string word;
        std::vector<std::string> words;

        // Split into words and filter out common words
        while (ss >> word) {
            // Convert to lowercase for comparison
            std::string lower_word = word;
            std::transform(lower_word.begin(), lower_word.end(), lower_word.begin(), ::tolower);

            // Skip common words that don't add meaning
            if (lower_word != "a" && lower_word != "an" && lower_word != "the"
                && lower_word != "and" && lower_word != "or" && lower_word != "but"
                && lower_word != "with" && lower_word != "for" && lower_word != "of"
                && lower_word != "in" && lower_word != "on" && lower_word != "at"
                && lower_word != "to" && lower_word != "from" && lower_word != "by"
                && lower_word != "logo" && lower_word != "design") {
                words.push_back(word);
            }
        }

        // Take first 2-3 meaningful words
        std::string clean_desc;
        int word_count = std::min(3, static_cast<int>(words.size()));
        for (int i = 0; i < word_count; ++i) {
            if (i > 0)
                clean_desc += "_";
            clean_desc += words[i];
        }

        // If no meaningful words found, use first word
        if (clean_desc.empty() && !words.empty()) {
            clean_desc = words[0];
        }

        // If still empty, use "logo"
        if (clean_desc.empty()) {
            clean_desc = "logo";
        }

        // Clean up special characters
        std::replace(clean_desc.begin(), clean_desc.end(), ' ', '_');
        std::replace(clean_desc.begin(), clean_desc.end(), '.', '_');
        std::replace(clean_desc.begin(), clean_desc.end(), ',', '_');
        std::replace(clean_desc.begin(), clean_desc.end(), '!', '_');
        std::replace(clean_desc.begin(), clean_desc.end(), '?', '_');
        std::replace(clean_desc.begin(), clean_desc.end(), '-', '_');

        // Add timestamp
        auto now = std::time(nullptr);
        auto tm = *std::localtime(&now);
        std::stringstream filename_ss;
        filename_ss << clean_desc << "_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << "." << format;

        return filename_ss.str();
    }

    std::string cleanSVGContent(const std::string& content) {
        std::string cleaned = content;

        // Remove markdown code blocks if present
        size_t start = cleaned.find("```");
        if (start != std::string::npos) {
            cleaned = cleaned.substr(start + 3);
            size_t end = cleaned.find("```");
            if (end != std::string::npos) {
                cleaned = cleaned.substr(0, end);
            }
        }

        // Fix common SVG namespace issues
        size_t ns_pos = cleaned.find("xmlns=\"http://www.w3.org/");
        if (ns_pos != std::string::npos) {
            size_t end_pos = cleaned.find("\"", ns_pos + 25);
            if (end_pos != std::string::npos) {
                std::string before = cleaned.substr(0, ns_pos + 25);
                std::string after = cleaned.substr(end_pos);
                cleaned = before + "2000/svg" + after;
            }
        }

        // Remove any leading/trailing whitespace
        cleaned.erase(0, cleaned.find_first_not_of(" \t\n\r"));
        cleaned.erase(cleaned.find_last_not_of(" \t\n\r") + 1);

        return cleaned;
    }

    bool isValidSVG(const std::string& content) {
        // Basic SVG validation
        return content.find("<?xml") != std::string::npos
               && content.find("<svg") != std::string::npos
               && content.find("</svg>") != std::string::npos;
    }

    void convertToRaster(const std::string& svg_path,
                         const std::string& output_path,
                         const std::string& format) {
        // Try to use ImageMagick or Inkscape for conversion
        std::string convert_cmd;

        if (format == "png") {
            convert_cmd = "convert " + svg_path + " " + output_path;
        } else if (format == "jpeg") {
            convert_cmd = "convert " + svg_path + " " + output_path;
        }

        std::cout << "🔄 Converting to " << format << " format..." << std::endl;

        int result = std::system(convert_cmd.c_str());
        if (result == 0) {
            std::cout << "✓ " << format << " logo saved to: " << output_path << std::endl;
        } else {
            std::cout << "⚠️  Conversion failed. SVG file is available at: " << svg_path
                      << std::endl;
            std::cout
                << "💡 Install ImageMagick for automatic conversion: sudo apt install imagemagick"
                << std::endl;
        }
    }

    bool handleCommand(const std::string& input) {
        std::string cmd = input;
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

        if (cmd == "quit" || cmd == "exit" || cmd == "bye") {
            std::cout << "👋 Goodbye! Thanks for using LogoGenerator!" << std::endl;
            exit(0);
        } else if (cmd == "help") {
            showHelp();
            return true;
        } else if (cmd == "list") {
            listGeneratedLogos();
            return true;
        } else if (cmd == "clear") {
            clearOutputDirectory();
            return true;
        }

        return false;
    }

    void showHelp() {
        std::cout << "\n📋 Available Commands:" << std::endl;
        std::cout << "  help     - Show this help message" << std::endl;
        std::cout << "  list     - List all generated logos" << std::endl;
        std::cout << "  clear    - Clear output directory" << std::endl;
        std::cout << "  quit/exit/bye - End session" << std::endl;
        std::cout << "\n💡 Tips:" << std::endl;
        std::cout << "  - Be specific about style, colors, and mood" << std::endl;
        std::cout << "  - Mention target audience (business, tech, creative, etc.)" << std::endl;
        std::cout << "  - Include industry context if relevant" << std::endl;
        std::cout << std::endl;
    }

    void listGeneratedLogos() {
        std::cout << "\n📁 Generated Logos:" << std::endl;

        try {
            for (const auto& entry : std::filesystem::directory_iterator(output_dir_)) {
                if (entry.is_regular_file()) {
                    std::cout << "  " << entry.path().filename().string() << std::endl;
                }
            }
        } catch (const std::exception& e) {
            std::cout << "❌ Error listing files: " << e.what() << std::endl;
        }
    }

    void clearOutputDirectory() {
        try {
            for (const auto& entry : std::filesystem::directory_iterator(output_dir_)) {
                if (entry.is_regular_file()) {
                    std::filesystem::remove(entry.path());
                }
            }
            std::cout << "🧹 Output directory cleared!" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "❌ Error clearing directory: " << e.what() << std::endl;
        }
    }
};

void printWelcome() {
    std::cout << "🎨 LLMEngine Logo Generator" << std::endl;
    std::cout << "===========================" << std::endl;
    std::cout << "Generate professional logos from text descriptions using AI." << std::endl;
    std::cout << "Supports PNG and JPEG output formats." << std::endl;
    std::cout << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  ./logo_generator \"Modern tech startup logo\" png" << std::endl;
    std::cout << "  ./logo_generator -i                    # Interactive mode" << std::endl;
    std::cout << "  ./logo_generator -m \"desc1\" \"desc2\"   # Multiple logos" << std::endl;
    std::cout << "  ./logo_generator ollama llama2         # Use local Ollama" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  \"Minimalist coffee shop logo with warm colors\"" << std::endl;
    std::cout << "  \"Tech company logo with geometric shapes and blue gradient\"" << std::endl;
    std::cout << "  \"Vintage restaurant logo with elegant typography\"" << std::endl;
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    printWelcome();

    // Check if using Ollama first (no API key needed)
    if (argc > 1 && std::string(argv[1]) == "ollama") {
        try {
            std::string ollama_model = argc > 2 ? argv[2] : "llama2";

            LogoGenerator generator("ollama", "", ollama_model, false);

            if (argc > 3 && std::string(argv[3]) == "-i") {
                generator.interactiveMode();
            } else if (argc > 3 && std::string(argv[3]) == "-m") {
                // Multiple logos mode
                std::vector<std::string> descriptions;
                for (int i = 4; i < argc; ++i) {
                    descriptions.push_back(argv[i]);
                }
                if (descriptions.empty()) {
                    std::cerr << "❌ No descriptions provided for multiple logo "
                                 "generation"
                              << std::endl;
                    return 1;
                }
                generator.generateMultipleLogos(descriptions);
            } else if (argc > 3) {
                // Single logo generation
                std::string description = argv[3];
                std::string format = argc > 4 ? argv[4] : "png";
                generator.generateLogo(description, format);
            } else {
                generator.interactiveMode();
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
        std::cerr << "   ./logo_generator ollama" << std::endl;
        return 1;
    }

    try {
        // Determine provider and model for online providers
        std::string provider = "qwen";
        std::string model = "qwen-flash";

        if (argc > 1) {
            provider = argv[1];
        }
        if (argc > 2) {
            model = argv[2];
        }

        LogoGenerator generator(provider, api_key, model, false);

        // Handle different modes
        if (argc > 3 && std::string(argv[3]) == "-i") {
            generator.interactiveMode();
        } else if (argc > 3 && std::string(argv[3]) == "-m") {
            // Multiple logos mode
            std::vector<std::string> descriptions;
            for (int i = 4; i < argc; ++i) {
                descriptions.push_back(argv[i]);
            }
            if (descriptions.empty()) {
                std::cerr << "❌ No descriptions provided for multiple logo "
                             "generation"
                          << std::endl;
                return 1;
            }
            generator.generateMultipleLogos(descriptions);
        } else if (argc > 3) {
            // Single logo generation
            std::string description = argv[3];
            std::string format = argc > 4 ? argv[4] : "png";
            generator.generateLogo(description, format);
        } else {
            generator.interactiveMode();
        }

    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
