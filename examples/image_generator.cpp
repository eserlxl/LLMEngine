// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This example demonstrates image generation via LLMEngine helpers and is
// licensed under the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

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
#include <map>

class ImageGenerator {
private:
    std::unique_ptr<LLMEngine> engine_;
    bool debug_mode_;
    std::string output_dir_;
    
    // Image type configurations
    std::map<std::string, std::string> image_types_;
    
public:
    ImageGenerator(const std::string& provider_name, const std::string& api_key, 
                  const std::string& model = "", bool debug = false) 
        : debug_mode_(debug) {
        try {
            // Configure parameters optimized for creative image generation
            nlohmann::json image_params = {
                {"temperature", 0.8},        // Higher creativity for image design
                {"max_tokens", 32768},        // More tokens for detailed SVG code
                {"top_p", 0.9},              // Good balance of creativity and coherence
                {"frequency_penalty", 0.1},   // Slight penalty to avoid repetition
                {"presence_penalty", 0.0},   // No penalty for introducing new concepts
                {"think", true}              // Enable chain of thought reasoning
            };
            
            engine_ = std::make_unique<LLMEngine>(provider_name, api_key, model, 
                                                 image_params, 24, debug);
            output_dir_ = "generated_images";
            std::filesystem::create_directories(output_dir_);
            
            // Initialize image type configurations
            initializeImageTypes();
            
            std::cout << "✓ ImageGenerator initialized with " << engine_->getProviderName() 
                      << " (" << (engine_->isOnlineProvider() ? "Online" : "Local") << ")" 
                      << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "❌ Failed to initialize ImageGenerator: " << e.what() << std::endl;
            throw;
        }
    }
    
    void generateImage(const std::string& description, const std::string& image_type = "artwork", 
                      const std::string& format = "png", const std::string& filename = "") {
        std::cout << "\n🎨 Generating " << image_type << " image from description: \"" << description << "\"" << std::endl;
        
        try {
            // Create prompt for SVG generation
            std::string prompt = createImagePrompt(description, image_type);
            
            // Generate SVG image code from LLM
            auto result = engine_->analyze(prompt, nlohmann::json{}, "image_generation");
            std::string svg_content = result[1];
            
            // Process the SVG response and convert to PNG
            processSVGResponse(svg_content, description, image_type, filename);
            
            std::cout << "🎉 Image generation completed!" << std::endl;
            std::cout << "📁 Output directory: " << output_dir_ << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "❌ Error generating image: " << e.what() << std::endl;
        }
    }
    
    void generateMultipleImages(const std::vector<std::string>& descriptions, 
                               const std::string& image_type = "artwork") {
        std::cout << "\n🎨 Generating " << descriptions.size() << " " << image_type << " images..." << std::endl;
        
        for (size_t i = 0; i < descriptions.size(); ++i) {
            std::cout << "\n--- Image " << (i + 1) << "/" << descriptions.size() << " ---" << std::endl;
            generateImage(descriptions[i], image_type);
        }
        
        std::cout << "\n🎉 All images generated successfully!" << std::endl;
    }
    
    void generateImageFromFile(const std::string& input_file, const std::string& image_type = "artwork", 
                              const std::string& output_filename = "") {
        std::cout << "\n📄 Reading prompt from file: " << input_file << std::endl;
        
        try {
            // Check if input file exists
            if (!std::filesystem::exists(input_file)) {
                std::cerr << "❌ Input file not found: " << input_file << std::endl;
                return;
            }
            
            // Read prompt from file
            std::ifstream input_stream(input_file);
            if (!input_stream.is_open()) {
                std::cerr << "❌ Failed to open input file: " << input_file << std::endl;
                return;
            }
            
            std::stringstream prompt_buffer;
            prompt_buffer << input_stream.rdbuf();
            input_stream.close();
            
            std::string prompt = prompt_buffer.str();
            
            // Remove trailing whitespace
            prompt.erase(prompt.find_last_not_of(" \t\n\r") + 1);
            
            if (prompt.empty()) {
                std::cerr << "❌ Input file is empty: " << input_file << std::endl;
                return;
            }
            
            std::cout << "✓ Prompt loaded from file (" << prompt.length() << " characters)" << std::endl;
            std::cout << "📝 Prompt: \"" << prompt << "\"" << std::endl;
            
            // Generate image with the prompt from file
            generateImage(prompt, image_type, "png", output_filename);
            
        } catch (const std::exception& e) {
            std::cerr << "❌ Error reading from file: " << e.what() << std::endl;
        }
    }
    
    void improveSVG(const std::string& svg_path, const std::string& improvement_prompt, 
                   const std::string& output_filename = "") {
        std::cout << "\n🔧 Improving SVG file: " << svg_path << std::endl;
        std::cout << "📝 Improvement prompt: \"" << improvement_prompt << "\"" << std::endl;
        
        try {
            // Check if SVG file exists
            if (!std::filesystem::exists(svg_path)) {
                std::cerr << "❌ SVG file not found: " << svg_path << std::endl;
                return;
            }
            
            // Read existing SVG content
            std::ifstream svg_file(svg_path);
            if (!svg_file.is_open()) {
                std::cerr << "❌ Failed to open SVG file: " << svg_path << std::endl;
                return;
            }
            
            std::stringstream svg_buffer;
            svg_buffer << svg_file.rdbuf();
            svg_file.close();
            
            std::string original_svg = svg_buffer.str();
            std::cout << "✓ Original SVG loaded (" << original_svg.length() << " characters)" << std::endl;
            
            // Create improvement prompt
            std::string prompt = createImprovementPrompt(original_svg, improvement_prompt);
            
            // Generate improved SVG from LLM
            auto result = engine_->analyze(prompt, nlohmann::json{}, "svg_improvement");
            std::string improved_svg_content = result[1];
            
            // Process the improved SVG response
            processImprovedSVGResponse(improved_svg_content, svg_path, improvement_prompt, output_filename);
            
            std::cout << "🎉 SVG improvement completed!" << std::endl;
            std::cout << "📁 Output directory: " << output_dir_ << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "❌ Error improving SVG: " << e.what() << std::endl;
        }
    }
    
    void interactiveMode() {
        std::cout << "\n🎨 Interactive Image Generator" << std::endl;
        std::cout << "==============================" << std::endl;
        std::cout << "Type 'quit', 'exit', or 'bye' to end the session." << std::endl;
        std::cout << "Type 'help' for available commands." << std::endl;
        std::cout << "Type 'types' to see available image types." << std::endl;
        std::cout << "Type 'improve <svg_file> <prompt>' to improve an existing SVG file." << std::endl;
        std::cout << "Generates PNG images by converting SVG to PNG automatically." << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        
        std::string user_input;
        
        while (true) {
            std::cout << "\n🎨 Describe your image: ";
            std::cout.flush();
            
            if (!std::getline(std::cin, user_input)) {
                std::cout << "\n👋 Goodbye! Thanks for using ImageGenerator!" << std::endl;
                break;
            }
            
            if (user_input.empty()) continue;
            
            // Handle special commands
            if (handleCommand(user_input)) {
                continue;
            }
            
            // Generate image (always PNG)
            generateImage(user_input);
        }
    }
    
    void showImageTypes() {
        std::cout << "\n📋 Available Image Types:" << std::endl;
        for (const auto& type : image_types_) {
            std::cout << "  " << type.first << " - " << type.second << std::endl;
        }
        std::cout << std::endl;
    }
    
private:
    void initializeImageTypes() {
        image_types_["artwork"] = "General artistic illustrations and paintings";
        image_types_["logo"] = "Professional logos and brand designs";
        image_types_["icon"] = "Simple icons and symbols";
        image_types_["diagram"] = "Technical diagrams and flowcharts";
        image_types_["infographic"] = "Informational graphics and charts";
        image_types_["poster"] = "Posters and promotional materials";
        image_types_["banner"] = "Web banners and headers";
        image_types_["avatar"] = "Profile pictures and avatars";
        image_types_["illustration"] = "Book illustrations and artwork";
        image_types_["sketch"] = "Hand-drawn sketches and doodles";
        image_types_["pattern"] = "Repeating patterns and textures";
        image_types_["background"] = "Background images and wallpapers";
    }
    
    std::string createImagePrompt(const std::string& description, const std::string& image_type) {
        std::stringstream prompt;
        
        // Get image type description
        std::string type_description = image_types_.count(image_type) ? image_types_[image_type] : "artistic image";
        
        prompt << "Create a complete SVG " << image_type << " for: \"" << description << "\"\n\n";
        prompt << "Please generate a complete, valid SVG code that includes:\n";
        prompt << "1. Proper SVG header with appropriate width and height dimensions\n";
        
        // Add type-specific instructions
        if (image_type == "logo") {
            prompt << "2. Professional logo design elements (shapes, text, gradients, etc.)\n";
            prompt << "3. Clean, modern styling suitable for business use\n";
            prompt << "4. Scalable vector graphics optimized for various sizes\n";
        } else if (image_type == "icon") {
            prompt << "2. Simple, recognizable icon design\n";
            prompt << "3. Clear, minimal styling with strong contrast\n";
            prompt << "4. Optimized for small sizes and clarity\n";
        } else if (image_type == "diagram") {
            prompt << "2. Technical diagram elements (boxes, arrows, connections)\n";
            prompt << "3. Clear labeling and professional appearance\n";
            prompt << "4. Structured layout with proper spacing\n";
        } else if (image_type == "infographic") {
            prompt << "2. Informational graphics with charts and visual elements\n";
            prompt << "3. Clear data representation and visual hierarchy\n";
            prompt << "4. Engaging design with appropriate colors\n";
        } else if (image_type == "poster") {
            prompt << "2. Eye-catching poster design with strong visual impact\n";
            prompt << "3. Appropriate typography and layout\n";
            prompt << "4. Professional promotional appearance\n";
        } else if (image_type == "banner") {
            prompt << "2. Web banner design optimized for online display\n";
            prompt << "3. Appropriate dimensions and responsive elements\n";
            prompt << "4. Clear call-to-action elements\n";
        } else if (image_type == "avatar") {
            prompt << "2. Profile picture design suitable for social media\n";
            prompt << "3. Square format with centered composition\n";
            prompt << "4. Personal and recognizable appearance\n";
        } else if (image_type == "illustration") {
            prompt << "2. Detailed illustration with artistic elements\n";
            prompt << "3. Rich visual storytelling and composition\n";
            prompt << "4. Professional illustration quality\n";
        } else if (image_type == "sketch") {
            prompt << "2. Hand-drawn sketch style with organic lines\n";
            prompt << "3. Artistic, loose drawing technique\n";
            prompt << "4. Natural, sketchy appearance\n";
        } else if (image_type == "pattern") {
            prompt << "2. Repeating pattern design with seamless tiling\n";
            prompt << "3. Consistent motif and spacing\n";
            prompt << "4. Suitable for background or texture use\n";
        } else if (image_type == "background") {
            prompt << "2. Background image design with appropriate depth\n";
            prompt << "3. Subtle elements that don't interfere with content\n";
            prompt << "4. Professional wallpaper appearance\n";
        } else {
            prompt << "2. Professional design elements (shapes, text, gradients, etc.)\n";
            prompt << "3. Appropriate colors using hex codes\n";
            prompt << "4. Clean, modern styling\n";
        }
        
        prompt << "5. Appropriate colors using hex codes\n";
        prompt << "6. Text elements if needed (use appropriate fonts)\n\n";
        prompt << "Return ONLY the complete SVG code, starting with <?xml version=\"1.0\" encoding=\"UTF-8\"?> and ending with </svg>\n";
        prompt << "Do not include any explanations or additional text - just the SVG code.\n";
        prompt << "Make it professional, scalable, and visually appealing for " << type_description << ".";
        
        return prompt.str();
    }
    
    std::string createImprovementPrompt(const std::string& original_svg, const std::string& improvement_prompt) {
        std::stringstream prompt;
        
        prompt << "Please improve the following SVG code based on the improvement request.\n\n";
        prompt << "Original SVG:\n";
        prompt << original_svg << "\n\n";
        prompt << "Improvement Request: \"" << improvement_prompt << "\"\n\n";
        prompt << "Please provide an improved version of the SVG that:\n";
        prompt << "1. Maintains the core structure and elements of the original\n";
        prompt << "2. Incorporates the requested improvements\n";
        prompt << "3. Keeps the SVG valid and well-formed\n";
        prompt << "4. Preserves appropriate dimensions and styling\n";
        prompt << "5. Enhances visual appeal and functionality\n\n";
        prompt << "Return ONLY the complete improved SVG code, starting with <?xml version=\"1.0\" encoding=\"UTF-8\"?> and ending with </svg>\n";
        prompt << "Do not include any explanations or additional text - just the improved SVG code.\n";
        prompt << "Make sure the improved SVG is professional, scalable, and visually enhanced.";
        
        return prompt.str();
    }
    
    void processImprovedSVGResponse(const std::string& content, const std::string& original_path, 
                                   const std::string& improvement_prompt, const std::string& filename) {
        std::cout << "📋 Generated improved SVG content:" << std::endl;
        std::cout << content << std::endl;
        
        // Clean up the SVG content
        std::string svg_content = cleanSVGContent(content);
        
        // Validate SVG content
        if (!isValidSVG(svg_content)) {
            std::cerr << "⚠️  Warning: Generated improved SVG content may not be valid" << std::endl;
        }
        
        // Generate filename if not provided
        std::string output_filename = filename.empty() ? generateImprovedFilename(original_path, improvement_prompt) : filename;
        std::string output_path;
        
        // If filename is provided and contains path, use it directly; otherwise use output directory
        if (!filename.empty() && (filename.find('/') != std::string::npos || filename.find('\\') != std::string::npos)) {
            output_path = filename;
        } else {
            output_path = output_dir_ + "/" + output_filename;
        }
        
        // Save improved SVG file
        std::ofstream svg_file(output_path);
        if (svg_file.is_open()) {
            svg_file << svg_content;
            svg_file.close();
            std::cout << "✓ Improved SVG saved to: " << output_path << std::endl;
        }
        
        // Convert to PNG
        std::string png_path = output_path.substr(0, output_path.find_last_of('.')) + ".png";
        bool keep_svg = (output_path.substr(output_path.find_last_of('.')) == ".svg");
        convertToPNG(output_path, png_path, keep_svg);
    }
    
    std::string generateImprovedFilename(const std::string& original_path, const std::string& improvement_prompt) {
        // Extract base name from original path
        std::filesystem::path original_file(original_path);
        std::string base_name = original_file.stem().string();
        
        // Extract key words from improvement prompt
        std::stringstream ss(improvement_prompt);
        std::string word;
        std::vector<std::string> words;
        
        // Split into words and filter out common words
        while (ss >> word) {
            // Convert to lowercase for comparison
            std::string lower_word = word;
            std::transform(lower_word.begin(), lower_word.end(), lower_word.begin(), ::tolower);
            
            // Skip common words that don't add meaning
            if (lower_word != "a" && lower_word != "an" && lower_word != "the" && 
                lower_word != "and" && lower_word != "or" && lower_word != "but" &&
                lower_word != "with" && lower_word != "for" && lower_word != "of" &&
                lower_word != "in" && lower_word != "on" && lower_word != "at" &&
                lower_word != "to" && lower_word != "from" && lower_word != "by" &&
                lower_word != "make" && lower_word != "add" && lower_word != "change" &&
                lower_word != "improve" && lower_word != "enhance" && lower_word != "update") {
                words.push_back(word);
            }
        }
        
        // Take first 2 meaningful words from improvement prompt
        std::string improvement_desc;
        int word_count = std::min(2, static_cast<int>(words.size()));
        for (int i = 0; i < word_count; ++i) {
            if (i > 0) improvement_desc += "_";
            improvement_desc += words[i];
        }
        
        // Clean up special characters
        std::replace(improvement_desc.begin(), improvement_desc.end(), ' ', '_');
        std::replace(improvement_desc.begin(), improvement_desc.end(), '.', '_');
        std::replace(improvement_desc.begin(), improvement_desc.end(), ',', '_');
        std::replace(improvement_desc.begin(), improvement_desc.end(), '!', '_');
        std::replace(improvement_desc.begin(), improvement_desc.end(), '?', '_');
        std::replace(improvement_desc.begin(), improvement_desc.end(), '-', '_');
        
        // Add timestamp
        auto now = std::time(nullptr);
        auto tm = *std::localtime(&now);
        std::stringstream filename_ss;
        filename_ss << base_name << "_improved";
        if (!improvement_desc.empty()) {
            filename_ss << "_" << improvement_desc;
        }
        filename_ss << "_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".svg";
        
        return filename_ss.str();
    }
    
    void processSVGResponse(const std::string& content, const std::string& description, 
                           const std::string& image_type, const std::string& filename) {
        std::cout << "📋 Generated SVG content:" << std::endl;
        std::cout << content << std::endl;
        
        // Clean up the SVG content
        std::string svg_content = cleanSVGContent(content);
        
        // Validate SVG content
        if (!isValidSVG(svg_content)) {
            std::cerr << "⚠️  Warning: Generated SVG content may not be valid" << std::endl;
        }
        
        // Generate filename if not provided
        std::string output_filename = filename.empty() ? generateFilename(description, image_type, "png") : filename;
        std::string output_path;
        
        // If filename is provided and contains path, use it directly; otherwise use output directory
        if (!filename.empty() && (filename.find('/') != std::string::npos || filename.find('\\') != std::string::npos)) {
            output_path = filename;
        } else {
            output_path = output_dir_ + "/" + output_filename;
        }
        
        // Save SVG file temporarily
        std::string svg_path = output_path.substr(0, output_path.find_last_of('.')) + ".svg";
        std::ofstream svg_file(svg_path);
        if (svg_file.is_open()) {
            svg_file << svg_content;
            svg_file.close();
            std::cout << "✓ SVG image saved to: " << svg_path << std::endl;
        }
        
        // Always convert to PNG
        bool keep_svg = (output_path.substr(output_path.find_last_of('.')) == ".svg");
        convertToPNG(svg_path, output_path, keep_svg);
    }
    
    bool isSVGContent(const std::string& content) {
        std::string lower_content = content;
        std::transform(lower_content.begin(), lower_content.end(), lower_content.begin(), ::tolower);
        
        return (lower_content.find("<?xml") != std::string::npos && 
                lower_content.find("<svg") != std::string::npos) ||
               (lower_content.find("<svg") != std::string::npos && 
                lower_content.find("</svg>") != std::string::npos);
    }
    
    bool isASCIIArt(const std::string& content) {
        // Simple heuristic: if content has multiple lines with consistent character patterns
        std::istringstream iss(content);
        std::string line;
        int lines_with_patterns = 0;
        int total_lines = 0;
        
        while (std::getline(iss, line)) {
            total_lines++;
            if (line.length() > 10) { // Reasonable length for ASCII art
                // Check for common ASCII art characters
                if (line.find_first_of("█▄▀▐▌▀▄█▓▒░┌┐└┘├┤┬┴┼╔╗╚╝╠╣╦╩╬") != std::string::npos ||
                    line.find_first_of("+-|/\\*#@$%&") != std::string::npos) {
                    lines_with_patterns++;
                }
            }
        }
        
        return total_lines > 3 && (lines_with_patterns * 100 / total_lines) > 30;
    }
    
    bool isBase64ImageContent(const std::string& content) {
        // Check if content looks like base64-encoded image data
        // Base64 images typically start with data URLs or are long strings of base64 characters
        std::string trimmed = content;
        trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
        trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);
        
        // Check for data URL format
        if (trimmed.find("data:image/png;base64,") != std::string::npos ||
            trimmed.find("data:image/jpeg;base64,") != std::string::npos ||
            trimmed.find("data:image/jpg;base64,") != std::string::npos) {
            return true;
        }
        
        // Check if it's a long string of base64 characters (at least 100 chars)
        if (trimmed.length() > 100) {
            // Count valid base64 characters
            int valid_chars = 0;
            for (char c : trimmed) {
                if (std::isalnum(c) || c == '+' || c == '/' || c == '=') {
                    valid_chars++;
                }
            }
            // If more than 80% are valid base64 chars, likely base64 data
            return (valid_chars * 100 / trimmed.length()) > 80;
        }
        
        return false;
    }
    
    std::string detectImageFormat(const std::string& content) {
        std::string trimmed = content;
        trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
        trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);
        
        // Check for data URL format
        if (trimmed.find("data:image/png;base64,") != std::string::npos) {
            return "png";
        } else if (trimmed.find("data:image/jpeg;base64,") != std::string::npos ||
                   trimmed.find("data:image/jpg;base64,") != std::string::npos) {
            return "jpeg";
        }
        
        // For raw base64, we'll default to PNG
        return "png";
    }
    
    std::string extractBase64Data(const std::string& content) {
        std::string trimmed = content;
        trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
        trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);
        
        // If it's a data URL, extract the base64 part
        size_t base64_start = trimmed.find("base64,");
        if (base64_start != std::string::npos) {
            return trimmed.substr(base64_start + 7);
        }
        
        // Otherwise, assume it's raw base64
        return trimmed;
    }
    
    void processBase64ImageContent(const std::string& content, const std::string& description, 
                                  const std::string& image_type, const std::string& filename) {
        std::cout << "🎨 Detected base64 image content" << std::endl;
        
        // Detect the image format
        std::string detected_format = detectImageFormat(content);
        std::cout << "📷 Detected format: " << detected_format << std::endl;
        
        // Extract base64 data
        std::string base64_data = extractBase64Data(content);
        
        // Generate filename if not provided
        std::string output_filename = filename.empty() ? generateFilename(description, image_type, detected_format) : filename;
        std::string output_path = output_dir_ + "/" + output_filename;
        
        // Decode base64 and save as image file
        try {
            // Simple base64 decoding
            std::string decoded_data = decodeBase64(base64_data);
            
            std::ofstream image_file(output_path, std::ios::binary);
            if (image_file.is_open()) {
                image_file.write(decoded_data.c_str(), decoded_data.length());
                image_file.close();
                std::cout << "✓ " << detected_format << " image saved to: " << output_path << std::endl;
            } else {
                std::cerr << "❌ Failed to create image file: " << output_path << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "❌ Error decoding base64 image: " << e.what() << std::endl;
            // Fallback: save as text file
            std::string txt_filename = output_filename.substr(0, output_filename.find_last_of('.')) + ".txt";
            std::string txt_path = output_dir_ + "/" + txt_filename;
            std::ofstream txt_file(txt_path);
            if (txt_file.is_open()) {
                txt_file << "Base64 Image Data:\n" << base64_data;
                txt_file.close();
                std::cout << "✓ Base64 data saved as text to: " << txt_path << std::endl;
            }
        }
    }
    
    std::string decodeBase64(const std::string& encoded) {
        // Simple base64 decoding implementation
        const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string decoded;
        int val = 0, valb = -8;
        
        for (char c : encoded) {
            if (chars.find(c) == std::string::npos) break;
            val = (val << 6) + chars.find(c);
            valb += 6;
            if (valb >= 0) {
                decoded.push_back(char((val >> valb) & 0xFF));
                valb -= 8;
            }
        }
        return decoded;
    }
    
    void processASCIIArt(const std::string& content, const std::string& description, 
                        const std::string& image_type, const std::string& filename) {
        std::cout << "🎨 Detected ASCII art content" << std::endl;
        
        // Generate filename if not provided
        std::string output_filename = filename.empty() ? generateFilename(description, image_type, "txt") : filename;
        std::string output_path = output_dir_ + "/" + output_filename;
        
        // Save ASCII art file
        std::ofstream art_file(output_path);
        if (art_file.is_open()) {
            art_file << content;
            art_file.close();
            std::cout << "✓ ASCII art saved to: " << output_path << std::endl;
        }
    }
    
    void processTextDescription(const std::string& content, const std::string& description, 
                               const std::string& image_type, const std::string& filename) {
        std::cout << "📝 Detected text description content" << std::endl;
        
        // Generate filename if not provided
        std::string output_filename = filename.empty() ? generateFilename(description, image_type, "txt") : filename;
        std::string output_path = output_dir_ + "/" + output_filename;
        
        // Save text description file
        std::ofstream desc_file(output_path);
        if (desc_file.is_open()) {
            desc_file << "Image Description: " << description << "\n\n";
            desc_file << "Generated Content:\n";
            desc_file << content;
            desc_file.close();
            std::cout << "✓ Text description saved to: " << output_path << std::endl;
        }
    }
    
    std::string generateFilename(const std::string& description, const std::string& image_type, const std::string& format) {
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
            if (lower_word != "a" && lower_word != "an" && lower_word != "the" && 
                lower_word != "and" && lower_word != "or" && lower_word != "but" &&
                lower_word != "with" && lower_word != "for" && lower_word != "of" &&
                lower_word != "in" && lower_word != "on" && lower_word != "at" &&
                lower_word != "to" && lower_word != "from" && lower_word != "by" &&
                lower_word != "image" && lower_word != "picture" && lower_word != "design" &&
                lower_word != "artwork" && lower_word != "illustration") {
                words.push_back(word);
            }
        }
        
        // Take first 2-3 meaningful words
        std::string clean_desc;
        int word_count = std::min(3, static_cast<int>(words.size()));
        for (int i = 0; i < word_count; ++i) {
            if (i > 0) clean_desc += "_";
            clean_desc += words[i];
        }
        
        // If no meaningful words found, use first word
        if (clean_desc.empty() && !words.empty()) {
            clean_desc = words[0];
        }
        
        // If still empty, use image type
        if (clean_desc.empty()) {
            clean_desc = image_type;
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
        return content.find("<?xml") != std::string::npos &&
               content.find("<svg") != std::string::npos &&
               content.find("</svg>") != std::string::npos;
    }
    
    void convertToPNG(const std::string& svg_path, const std::string& png_path, bool keep_svg = false) {
        // Use ImageMagick to convert SVG to PNG
        std::string convert_cmd = "convert " + svg_path + " " + png_path;
        
        std::cout << "🔄 Converting SVG to PNG..." << std::endl;
        
        int result = std::system(convert_cmd.c_str());
        if (result == 0) {
            std::cout << "✓ PNG image saved to: " << png_path << std::endl;
            // Remove temporary SVG file only if not keeping it
            if (!keep_svg) {
                std::filesystem::remove(svg_path);
            }
        } else {
            std::cout << "⚠️  PNG conversion failed. SVG file is available at: " << svg_path << std::endl;
            std::cout << "💡 Install ImageMagick for automatic conversion: sudo apt install imagemagick" << std::endl;
        }
    }
    
    bool handleCommand(const std::string& input) {
        std::string cmd = input;
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
        
        if (cmd == "quit" || cmd == "exit" || cmd == "bye") {
            std::cout << "👋 Goodbye! Thanks for using ImageGenerator!" << std::endl;
            exit(0);
        }
        else if (cmd == "help") {
            showHelp();
            return true;
        }
        else if (cmd == "types") {
            showImageTypes();
            return true;
        }
        else if (cmd == "list") {
            listGeneratedImages();
            return true;
        }
        else if (cmd == "clear") {
            clearOutputDirectory();
            return true;
        }
        else if (cmd.substr(0, 8) == "improve ") {
            handleImproveCommand(input);
            return true;
        }
        
        return false;
    }
    
    void handleImproveCommand(const std::string& input) {
        // Parse "improve <svg_file> <prompt>" command
        std::istringstream iss(input);
        std::string cmd, svg_file, prompt;
        
        iss >> cmd >> svg_file;
        
        if (svg_file.empty()) {
            std::cout << "❌ Usage: improve <svg_file> <prompt>" << std::endl;
            std::cout << "Example: improve logo.svg add gradient colors" << std::endl;
            return;
        }
        
        // Get the rest as the improvement prompt
        std::string line;
        std::getline(iss, line);
        prompt = line;
        
        // Remove leading whitespace
        prompt.erase(0, prompt.find_first_not_of(" \t"));
        
        if (prompt.empty()) {
            std::cout << "❌ Please provide an improvement prompt" << std::endl;
            std::cout << "Example: improve logo.svg add gradient colors" << std::endl;
            return;
        }
        
        // Check if SVG file exists
        if (!std::filesystem::exists(svg_file)) {
            std::cout << "❌ SVG file not found: " << svg_file << std::endl;
            return;
        }
        
        // Call improveSVG method
        improveSVG(svg_file, prompt);
    }
    
    void showHelp() {
        std::cout << "\n📋 Available Commands:" << std::endl;
        std::cout << "  help     - Show this help message" << std::endl;
        std::cout << "  types    - Show available image types" << std::endl;
        std::cout << "  list     - List all generated images" << std::endl;
        std::cout << "  clear    - Clear output directory" << std::endl;
        std::cout << "  improve <svg_file> <prompt> - Improve existing SVG file" << std::endl;
        std::cout << "  quit/exit/bye - End session" << std::endl;
        std::cout << "\n💡 Tips:" << std::endl;
        std::cout << "  - Be specific about style, colors, and mood" << std::endl;
        std::cout << "  - Mention target audience (business, tech, creative, etc.)" << std::endl;
        std::cout << "  - Include industry context if relevant" << std::endl;
        std::cout << "  - Use 'types' command to see available image categories" << std::endl;
        std::cout << "  - Images are generated as SVG and automatically converted to PNG" << std::endl;
        std::cout << "  - Use 'improve' command to enhance existing SVG files with AI" << std::endl;
        std::cout << std::endl;
    }
    
    void listGeneratedImages() {
        std::cout << "\n📁 Generated Images:" << std::endl;
        
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
    std::cout << "🎨 LLMEngine Image Generator" << std::endl;
    std::cout << "============================" << std::endl;
    std::cout << "Generate various types of images from text descriptions using AI." << std::endl;
    std::cout << "Generates PNG images by converting SVG to PNG automatically." << std::endl;
    std::cout << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  ./image_generator \"Beautiful sunset landscape\" artwork" << std::endl;
    std::cout << "  ./image_generator -i                    # Interactive mode" << std::endl;
    std::cout << "  ./image_generator -m \"desc1\" \"desc2\"   # Multiple images" << std::endl;
    std::cout << "  ./image_generator -s file.svg \"prompt\"  # Improve SVG file" << std::endl;
    std::cout << "  ./image_generator -s file.svg -i prompt.txt  # Improve SVG with file prompt" << std::endl;
    std::cout << "  ./image_generator -i prompt.txt -o output.png  # Read from file, save to file" << std::endl;
    std::cout << "  ./image_generator ollama qwen3:1.7b    # Use local Ollama" << std::endl;
    std::cout << std::endl;
    std::cout << "Image Types:" << std::endl;
    std::cout << "  artwork, logo, icon, diagram, infographic, poster, banner," << std::endl;
    std::cout << "  avatar, illustration, sketch, pattern, background" << std::endl;
    std::cout << std::endl;
    std::cout << "Output:" << std::endl;
    std::cout << "  PNG images (converted from SVG automatically)" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  \"Minimalist coffee shop logo with warm colors\" logo" << std::endl;
    std::cout << "  \"Tech company diagram showing data flow\" diagram" << std::endl;
    std::cout << "  \"Vintage poster for music festival\" poster" << std::endl;
    std::cout << "  \"Abstract pattern with geometric shapes\" pattern" << std::endl;
    std::cout << "  -s logo.svg \"add gradient colors and shadows\"" << std::endl;
    std::cout << "  -s diagram.svg \"make it more colorful and modern\"" << std::endl;
    std::cout << "  -s logo.svg -i improvement.txt -o new_logo.svg" << std::endl;
    std::cout << "  -i prompt.txt -o my_image.png" << std::endl;
    std::cout << "  -i description.txt -o /path/to/output.svg" << std::endl;
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    printWelcome();
    
    // Parse command line arguments
    std::string provider = "qwen";
    std::string model = "qwen-flash";
    std::string input_file = "";
    std::string output_file = "";
    std::string mode = "";
    std::vector<std::string> descriptions;
    std::string svg_file = "";
    std::string improvement_prompt = "";
    std::string image_type = "artwork";
    
    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "ollama") {
            provider = "ollama";
            model = (i + 1 < argc) ? argv[++i] : "llama2";
        } else if (arg == "-i") {
            if (i + 1 < argc) {
                input_file = argv[++i];
            } else {
                mode = "interactive";
            }
        } else if (arg == "-o") {
            if (i + 1 < argc) {
                output_file = argv[++i];
            } else {
                std::cerr << "❌ -o option requires a filename" << std::endl;
                return 1;
            }
        } else if (arg == "-m") {
            mode = "multiple";
            // Collect remaining arguments as descriptions
            for (int j = i + 1; j < argc; ++j) {
                descriptions.push_back(argv[j]);
            }
            break;
        } else if (arg == "-s") {
            mode = "improve";
            if (i + 1 < argc) {
                svg_file = argv[++i];
            } else {
                std::cerr << "❌ -s option requires SVG file path" << std::endl;
                return 1;
            }
            // Check if next argument is -i (read prompt from file) or direct prompt
            if (i + 1 < argc && std::string(argv[i + 1]) == "-i") {
                // Skip -i and get the file path
                i += 2;
                if (i < argc) {
                    std::string prompt_file = argv[i];
                    // Read improvement prompt from file
                    try {
                        if (!std::filesystem::exists(prompt_file)) {
                            std::cerr << "❌ Improvement prompt file not found: " << prompt_file << std::endl;
                            return 1;
                        }
                        
                        std::ifstream prompt_stream(prompt_file);
                        if (!prompt_stream.is_open()) {
                            std::cerr << "❌ Failed to open improvement prompt file: " << prompt_file << std::endl;
                            return 1;
                        }
                        
                        std::stringstream prompt_buffer;
                        prompt_buffer << prompt_stream.rdbuf();
                        prompt_stream.close();
                        
                        improvement_prompt = prompt_buffer.str();
                        
                        // Remove trailing whitespace
                        improvement_prompt.erase(improvement_prompt.find_last_not_of(" \t\n\r") + 1);
                        
                        if (improvement_prompt.empty()) {
                            std::cerr << "❌ Improvement prompt file is empty: " << prompt_file << std::endl;
                            return 1;
                        }
                        
                        std::cout << "✓ Improvement prompt loaded from file: " << prompt_file << std::endl;
                    } catch (const std::exception& e) {
                        std::cerr << "❌ Error reading improvement prompt file: " << e.what() << std::endl;
                        return 1;
                    }
                } else {
                    std::cerr << "❌ -i option requires improvement prompt file path" << std::endl;
                    return 1;
                }
            } else if (i + 1 < argc) {
                improvement_prompt = argv[++i];
            } else {
                std::cerr << "❌ -s option requires improvement prompt (direct or via -i file)" << std::endl;
                return 1;
            }
            // Don't break here - continue parsing for -o option
        } else if (arg[0] != '-') {
            // This is a description or image type
            if (mode.empty() && descriptions.empty()) {
                descriptions.push_back(arg);
            } else if (descriptions.size() == 1 && image_type == "artwork") {
                image_type = arg;
            }
        }
    }
    
    // Check if using Ollama first (no API key needed)
    if (provider == "ollama") {
        try {
            ImageGenerator generator("ollama", "", model, false);
            
            if (mode == "interactive") {
                generator.interactiveMode();
            } else if (mode == "multiple") {
                if (descriptions.empty()) {
                    std::cerr << "❌ No descriptions provided for multiple image generation" << std::endl;
                    return 1;
                }
                generator.generateMultipleImages(descriptions);
            } else if (mode == "improve") {
                generator.improveSVG(svg_file, improvement_prompt, output_file);
            } else if (!input_file.empty()) {
                generator.generateImageFromFile(input_file, image_type, output_file);
            } else if (!descriptions.empty()) {
                generator.generateImage(descriptions[0], image_type, "png", output_file);
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
        std::cerr << "   ./image_generator ollama" << std::endl;
        return 1;
    }
    
    try {
        ImageGenerator generator(provider, api_key, model, false);
        
        if (mode == "interactive") {
            generator.interactiveMode();
        } else if (mode == "multiple") {
            if (descriptions.empty()) {
                std::cerr << "❌ No descriptions provided for multiple image generation" << std::endl;
                return 1;
            }
            generator.generateMultipleImages(descriptions);
        } else if (mode == "improve") {
            generator.improveSVG(svg_file, improvement_prompt, output_file);
        } else if (!input_file.empty()) {
            generator.generateImageFromFile(input_file, image_type, output_file);
        } else if (!descriptions.empty()) {
            generator.generateImage(descriptions[0], image_type, "png", output_file);
        } else {
            generator.interactiveMode();
        }
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
