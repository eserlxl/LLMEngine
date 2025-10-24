#include "LLMEngine.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cstdlib>

class LogoGenerator {
private:
    std::unique_ptr<LLMEngine> engine_;
    bool debug_mode_;
    std::string output_dir_;
    
public:
    LogoGenerator(const std::string& provider_name, const std::string& api_key, 
                  const std::string& model = "", bool debug = false) 
        : debug_mode_(debug) {
        try {
            engine_ = std::make_unique<LLMEngine>(provider_name, api_key, model, 
                                                 nlohmann::json{}, 24, debug);
            output_dir_ = "generated_logos";
            std::filesystem::create_directories(output_dir_);
            
            std::cout << "✓ LogoGenerator initialized with " << engine_->getProviderName() 
                      << " (" << (engine_->isOnlineProvider() ? "Online" : "Local") << ")" 
                      << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "❌ Failed to initialize LogoGenerator: " << e.what() << std::endl;
            throw;
        }
    }
    
    void generateLogo(const std::string& description, const std::string& format = "png", 
                     const std::string& filename = "") {
        std::cout << "\n🎨 Generating logo from description: \"" << description << "\"" << std::endl;
        
        try {
            // Create detailed prompt for logo generation
            std::string prompt = createLogoPrompt(description, format);
            
            // Generate logo description and specifications
            auto result = engine_->analyze(prompt, nlohmann::json{}, "logo_generation");
            std::string logo_spec = result[1];
            
            std::cout << "📋 Generated logo specifications:" << std::endl;
            std::cout << logo_spec << std::endl;
            
            // Generate filename if not provided
            std::string output_filename = filename.empty() ? generateFilename(description, format) : filename;
            std::string output_path = output_dir_ + "/" + output_filename;
            
            // Create SVG content (we'll generate SVG and then convert to PNG/JPEG)
            std::string svg_content = generateSVGContent(logo_spec, description);
            
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
            std::cout << "\n--- Logo " << (i + 1) << "/" << descriptions.size() << " ---" << std::endl;
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
            
            if (user_input.empty()) continue;
            
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
        prompt << "Create a detailed logo design specification for: \"" << description << "\"\n\n";
        prompt << "Please provide:\n";
        prompt << "1. Logo concept and style (minimalist, modern, vintage, etc.)\n";
        prompt << "2. Color scheme (primary and secondary colors)\n";
        prompt << "3. Typography suggestions (font style, size hierarchy)\n";
        prompt << "4. Layout and composition details\n";
        prompt << "5. Icon/symbol suggestions\n";
        prompt << "6. Overall visual theme and mood\n\n";
        prompt << "Format the response as a structured specification that can be used to create an SVG logo.\n";
        prompt << "Be specific about dimensions, colors (use hex codes), and design elements.\n";
        prompt << "Make it professional and suitable for business use.";
        
        return prompt.str();
    }
    
    std::string generateFilename(const std::string& description, const std::string& format) {
        // Clean description for filename
        std::string clean_desc = description;
        std::replace(clean_desc.begin(), clean_desc.end(), ' ', '_');
        std::replace(clean_desc.begin(), clean_desc.end(), '.', '_');
        std::replace(clean_desc.begin(), clean_desc.end(), ',', '_');
        std::replace(clean_desc.begin(), clean_desc.end(), '!', '_');
        std::replace(clean_desc.begin(), clean_desc.end(), '?', '_');
        
        // Limit length
        if (clean_desc.length() > 30) {
            clean_desc = clean_desc.substr(0, 30);
        }
        
        // Add timestamp
        auto now = std::time(nullptr);
        auto tm = *std::localtime(&now);
        std::stringstream ss;
        ss << clean_desc << "_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << "." << format;
        
        return ss.str();
    }
    
    std::string generateSVGContent(const std::string& spec, const std::string& description) {
        // This is a simplified SVG generator - in a real implementation,
        // you would parse the AI-generated spec and create more sophisticated SVG
        
        std::stringstream svg;
        svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        svg << "<svg width=\"400\" height=\"200\" xmlns=\"http://www.w3.org/2000/svg\">\n";
        svg << "  <defs>\n";
        svg << "    <linearGradient id=\"grad1\" x1=\"0%\" y1=\"0%\" x2=\"100%\" y2=\"100%\">\n";
        svg << "      <stop offset=\"0%\" style=\"stop-color:#4A90E2;stop-opacity:1\" />\n";
        svg << "      <stop offset=\"100%\" style=\"stop-color:#7B68EE;stop-opacity:1\" />\n";
        svg << "    </linearGradient>\n";
        svg << "  </defs>\n";
        svg << "  <!-- Background -->\n";
        svg << "  <rect width=\"400\" height=\"200\" fill=\"#ffffff\" stroke=\"#e0e0e0\" stroke-width=\"1\"/>\n";
        svg << "  <!-- Main shape -->\n";
        svg << "  <circle cx=\"100\" cy=\"100\" r=\"60\" fill=\"url(#grad1)\" opacity=\"0.8\"/>\n";
        svg << "  <!-- Text -->\n";
        svg << "  <text x=\"200\" y=\"120\" font-family=\"Arial, sans-serif\" font-size=\"24\" font-weight=\"bold\" fill=\"#333333\">\n";
        svg << "    " << (description.length() > 20 ? description.substr(0, 20) + "..." : description) << "\n";
        svg << "  </text>\n";
        svg << "  <!-- Decorative elements -->\n";
        svg << "  <rect x=\"180\" y=\"80\" width=\"120\" height=\"4\" fill=\"#4A90E2\" opacity=\"0.6\"/>\n";
        svg << "  <rect x=\"180\" y=\"140\" width=\"80\" height=\"2\" fill=\"#7B68EE\" opacity=\"0.6\"/>\n";
        svg << "</svg>\n";
        
        return svg.str();
    }
    
    void convertToRaster(const std::string& svg_path, const std::string& output_path, 
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
            std::cout << "⚠️  Conversion failed. SVG file is available at: " << svg_path << std::endl;
            std::cout << "💡 Install ImageMagick for automatic conversion: sudo apt install imagemagick" << std::endl;
        }
    }
    
    bool handleCommand(const std::string& input) {
        std::string cmd = input;
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
        
        if (cmd == "quit" || cmd == "exit" || cmd == "bye") {
            std::cout << "👋 Goodbye! Thanks for using LogoGenerator!" << std::endl;
            exit(0);
        }
        else if (cmd == "help") {
            showHelp();
            return true;
        }
        else if (cmd == "list") {
            listGeneratedLogos();
            return true;
        }
        else if (cmd == "clear") {
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
                    std::cerr << "❌ No descriptions provided for multiple logo generation" << std::endl;
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
                std::cerr << "❌ No descriptions provided for multiple logo generation" << std::endl;
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
