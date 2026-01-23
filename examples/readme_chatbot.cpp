// Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This example builds a README-based chatbot using LLMEngine and is
// licensed under the GNU General Public License v3.0 or later.
// See the LICENSE file in the project root for details.

#include "LLMEngine.hpp"

#include <algorithm>
#include <cpr/cpr.h>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace LLMEngine;

class ReadmeChatBot {
  private:
    std::unique_ptr<LLMEngine::LLMEngine> engine_;
    std::string readme_content_;
    std::string project_url_;
    std::string project_name_;
    bool debug_mode_;

  public:
    ReadmeChatBot(const std::string& provider_name,
                  const std::string& api_key,
                  const std::string& model = "",
                  bool debug = false)
        : debug_mode_(debug) {
        try {
            // Configure parameters optimized for chat interactions
            nlohmann::json chat_params = {
                {"temperature", 0.7},       // Balanced creativity
                {"max_tokens", 32768},      // Reasonable response length
                {"top_p", 0.9},             // Good balance of creativity and coherence
                {"frequency_penalty", 0.1}, // Slight penalty to avoid repetition
                {"presence_penalty", 0.0},  // No penalty for introducing new concepts
                {"think", true}             // Enable chain of thought reasoning
            };

            engine_ = std::make_unique<LLMEngine::LLMEngine>(
                provider_name, api_key, model, chat_params, 24, debug);
            std::cout << "✓ ReadmeChatBot initialized with " << engine_->getProviderName() << " ("
                      << (engine_->isOnlineProvider() ? "Online" : "Local") << ")" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "❌ Failed to initialize ReadmeChatBot: " << e.what() << std::endl;
            throw;
        }
    }

    bool loadReadmeFromUrl(const std::string& url) {
        try {
            // Clean and normalize the URL
            std::string clean_url = normalizeGitHubUrl(url);
            project_url_ = clean_url;

            // Extract project name from URL
            project_name_ = extractProjectName(clean_url);

            std::cout << "📖 Loading README from: " << clean_url << std::endl;

            // Fetch the README content
            auto response = cpr::Get(cpr::Url{clean_url});

            if (response.status_code != 200) {
                std::cerr << "❌ Failed to fetch README. HTTP Status: " << response.status_code
                          << std::endl;
                return false;
            }

            readme_content_ = response.text;

            if (readme_content_.empty()) {
                std::cerr << "❌ README content is empty!" << std::endl;
                return false;
            }

            // Clean and process the markdown content
            readme_content_ = cleanMarkdownContent(readme_content_);

            std::cout << "✅ Successfully loaded README (" << readme_content_.length()
                      << " characters)" << std::endl;
            std::cout << "📋 Project: " << project_name_ << std::endl;

            return true;

        } catch (const std::exception& e) {
            std::cerr << "❌ Error loading README: " << e.what() << std::endl;
            return false;
        }
    }

    bool loadReadmeFromFile(const std::string& filepath) {
        try {
            std::ifstream file(filepath);
            if (!file.is_open()) {
                std::cerr << "❌ Could not open file: " << filepath << std::endl;
                return false;
            }

            std::stringstream buffer;
            buffer << file.rdbuf();
            readme_content_ = buffer.str();
            file.close();

            if (readme_content_.empty()) {
                std::cerr << "❌ README file is empty!" << std::endl;
                return false;
            }

            // Extract project name from filepath
            project_name_ = extractProjectNameFromPath(filepath);
            project_url_ = "local file: " + filepath;

            // Clean and process the markdown content
            readme_content_ = cleanMarkdownContent(readme_content_);

            std::cout << "✅ Successfully loaded README from file (" << readme_content_.length()
                      << " characters)" << std::endl;
            std::cout << "📋 Project: " << project_name_ << std::endl;

            return true;

        } catch (const std::exception& e) {
            std::cerr << "❌ Error loading README file: " << e.what() << std::endl;
            return false;
        }
    }

    void startConversation() {
        if (readme_content_.empty()) {
            std::cerr << "❌ No README content loaded! Please load a README first." << std::endl;
            return;
        }

        std::cout << "\n🤖 ReadmeChatBot started! Ask questions about the project." << std::endl;
        std::cout << "💡 Try: 'help' for commands, 'summary' for project overview, 'quit' to exit"
                  << std::endl;
        std::cout << std::string(60, '=') << std::endl;

        std::string user_input;
        while (true) {
            std::cout << "\n👤 You: ";
            std::cout.flush();

            if (!std::getline(std::cin, user_input)) {
                std::cout << "\n👋 Goodbye! Thanks for chatting!" << std::endl;
                break;
            }

            if (user_input.empty())
                continue;

            // Handle special commands
            if (handleCommand(user_input)) {
                continue;
            }

            try {
                // Create context-aware prompt
                std::string context_prompt = createContextPrompt(user_input);

                // Get response from LLM
                AnalysisResult result =
                    engine_->analyze(context_prompt, nlohmann::json{}, "chat", "chat");
                std::string response = result.content;

                // Display response
                std::cout << "🤖 Bot: " << response << std::endl;

            } catch (const std::exception& e) {
                std::cerr << "❌ Error getting response: " << e.what() << std::endl;
                std::cout << "🤖 Bot: I'm sorry, I encountered an error. Please try again."
                          << std::endl;
            }
        }
    }

  private:
    std::string normalizeGitHubUrl(const std::string& url) {
        std::string normalized = url;

        // Remove trailing slash
        if (!normalized.empty() && normalized.back() == '/') {
            normalized.pop_back();
        }

        // If it's a GitHub project URL, append README.md
        if (normalized.find("github.com") != std::string::npos) {
            // Check if it already ends with README.md
            if (normalized.find("README.md") == std::string::npos) {
                // Check if it's a raw URL or regular URL
                if (normalized.find("/raw/") != std::string::npos) {
                    normalized += "/README.md";
                } else {
                    // Convert to raw URL
                    normalized =
                        std::regex_replace(normalized,
                                           std::regex("github\\.com/([^/]+)/([^/]+)"),
                                           "raw.githubusercontent.com/$1/$2/main/README.md");
                }
            }
        }

        return normalized;
    }

    std::string extractProjectName(const std::string& url) {
        std::regex repo_regex(R"(github\.com/([^/]+)/([^/]+))");
        std::smatch match;

        if (std::regex_search(url, match, repo_regex)) {
            return match[1].str() + "/" + match[2].str();
        }

        // Fallback: extract from URL path
        std::regex path_regex(R"(/([^/]+)/([^/]+))");
        if (std::regex_search(url, match, path_regex)) {
            return match[1].str() + "/" + match[2].str();
        }

        return "Unknown Project";
    }

    std::string extractProjectNameFromPath(const std::string& filepath) {
        std::filesystem::path path(filepath);
        std::string filename = path.filename().string();

        // Remove .md extension if present
        if (filename.length() > 3 && filename.substr(filename.length() - 3) == ".md") {
            filename = filename.substr(0, filename.length() - 3);
        }

        return filename;
    }

    std::string cleanMarkdownContent(const std::string& content) {
        std::string cleaned = content;

        // Remove HTML tags
        cleaned = std::regex_replace(cleaned, std::regex("<[^>]*>"), "");

        // Remove excessive whitespace
        cleaned = std::regex_replace(cleaned, std::regex("\\s+"), " ");

        // Remove markdown link syntax but keep the text
        cleaned = std::regex_replace(cleaned, std::regex("\\[([^\\]]+)\\]\\([^\\)]+\\)"), "$1");

        // Remove markdown headers but keep the text
        cleaned = std::regex_replace(cleaned, std::regex("#+\\s*"), "");

        // Remove markdown code blocks markers
        cleaned = std::regex_replace(cleaned, std::regex("```[^`]*```"), "[CODE BLOCK]");
        cleaned = std::regex_replace(cleaned, std::regex("`([^`]+)`"), "$1");

        // Trim whitespace
        cleaned.erase(0, cleaned.find_first_not_of(" \t\n\r"));
        cleaned.erase(cleaned.find_last_not_of(" \t\n\r") + 1);

        return cleaned;
    }

    std::string createContextPrompt(const std::string& user_question) {
        std::stringstream prompt;

        prompt << "You are a helpful assistant that answers questions about a software project "
                  "based on its README file.\n\n";
        prompt << "Project: " << project_name_ << "\n";
        prompt << "Source: " << project_url_ << "\n\n";
        prompt << "README Content:\n";
        prompt << readme_content_ << "\n\n";
        prompt << "User Question: " << user_question << "\n\n";
        prompt << "Please answer the user's question based on the information provided in the "
                  "README. ";
        prompt << "If the README doesn't contain enough information to answer the question, please "
                  "say so. ";
        prompt << "Be helpful and provide specific details from the README when possible.";

        return prompt.str();
    }

    bool handleCommand(const std::string& input) {
        std::string cmd = input;
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

        if (cmd == "quit" || cmd == "exit" || cmd == "bye") {
            std::cout << "👋 Goodbye! Thanks for chatting!" << std::endl;
            exit(0);
        } else if (cmd == "help") {
            showHelp();
            return true;
        } else if (cmd == "summary") {
            showProjectSummary();
            return true;
        } else if (cmd == "status") {
            showStatus();
            return true;
        } else if (cmd == "reload") {
            if (!project_url_.empty()) {
                std::cout << "🔄 Reloading README..." << std::endl;
                loadReadmeFromUrl(project_url_);
            } else {
                std::cout << "❌ No URL to reload from!" << std::endl;
            }
            return true;
        }

        return false;
    }

    void showHelp() {
        std::cout << "\n📋 Available Commands:" << std::endl;
        std::cout << "  help     - Show this help message" << std::endl;
        std::cout << "  summary  - Show project overview from README" << std::endl;
        std::cout << "  status   - Show bot status and project info" << std::endl;
        std::cout << "  reload   - Reload README from URL" << std::endl;
        std::cout << "  quit/exit/bye - End conversation" << std::endl;
        std::cout << std::endl;
        std::cout << "💡 You can ask questions like:" << std::endl;
        std::cout << "  - What is this project about?" << std::endl;
        std::cout << "  - How do I install this?" << std::endl;
        std::cout << "  - What are the main features?" << std::endl;
        std::cout << "  - How do I contribute?" << std::endl;
        std::cout << std::endl;
    }

    void showProjectSummary() {
        if (readme_content_.empty()) {
            std::cout << "❌ No README content loaded!" << std::endl;
            return;
        }

        try {
            std::string summary_prompt =
                "Based on the README content, provide a concise summary of what this project is "
                "about, its main features, and how to get started. Keep it under 200 words.";
            std::string context_prompt = createContextPrompt(summary_prompt);

            AnalysisResult result =
                engine_->analyze(context_prompt, nlohmann::json{}, "chat", "chat");
            std::string summary = result.content;

            std::cout << "\n📋 Project Summary:" << std::endl;
            std::cout << std::string(40, '-') << std::endl;
            std::cout << summary << std::endl;
            std::cout << std::string(40, '-') << std::endl;

        } catch (const std::exception& e) {
            std::cerr << "❌ Error generating summary: " << e.what() << std::endl;
        }
    }

    void showStatus() {
        std::cout << "\n📊 ReadmeChatBot Status:" << std::endl;
        std::cout << "  Provider: " << engine_->getProviderName() << std::endl;
        std::cout << "  Type: " << (engine_->isOnlineProvider() ? "Online API" : "Local")
                  << std::endl;
        std::cout << "  Debug Mode: " << (debug_mode_ ? "Enabled" : "Disabled") << std::endl;
        std::cout << "  Project: " << project_name_ << std::endl;
        std::cout << "  Source: " << project_url_ << std::endl;
        std::cout << "  README Length: " << readme_content_.length() << " characters" << std::endl;
        std::cout << std::endl;
    }
};

void printWelcome() {
    std::cout << "🤖 LLMEngine ReadmeChatBot Example" << std::endl;
    std::cout << "===================================" << std::endl;
    std::cout
        << "This chatbot can read GitHub project README files and answer questions about them."
        << std::endl;
    std::cout << "Supports multiple AI providers: Qwen, OpenAI, Anthropic, Ollama" << std::endl;
    std::cout << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  ./readme_chatbot <github_url> [provider] [model]" << std::endl;
    std::cout << "  ./readme_chatbot <readme_file> [provider] [model]" << std::endl;
    std::cout << "  ./readme_chatbot ollama <github_url> [model]" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  ./readme_chatbot https://github.com/user/repo" << std::endl;
    std::cout << "  ./readme_chatbot README.md ollama" << std::endl;
    std::cout << "  ./readme_chatbot https://github.com/user/repo qwen qwen-flash" << std::endl;
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    printWelcome();

    if (argc < 2) {
        std::cerr << "❌ Please provide a GitHub URL or README file path!" << std::endl;
        std::cerr << "Usage: " << argv[0] << " <url_or_file> [provider] [model]" << std::endl;
        return 1;
    }

    std::string input = argv[1];
    std::string provider = "qwen";
    std::string model = "qwen-flash";
    std::string api_key;

    // Check if using Ollama first
    if (argc > 2 && std::string(argv[2]) == "ollama") {
        try {
            std::string ollama_model = argc > 3 ? argv[3] : "llama2";

            ReadmeChatBot bot("ollama", "", ollama_model, false);

            // Determine if input is URL or file
            if (input.find("http") == 0) {
                if (!bot.loadReadmeFromUrl(input)) {
                    return 1;
                }
            } else {
                if (!bot.loadReadmeFromFile(input)) {
                    return 1;
                }
            }

            bot.startConversation();
            return 0;

        } catch (const std::exception& e) {
            std::cerr << "❌ Error: " << e.what() << std::endl;
            return 1;
        }
    }

    // Get API key from environment for online providers
    const char* env_api_key = std::getenv("QWEN_API_KEY");
    if (!env_api_key) {
        env_api_key = std::getenv("OPENAI_API_KEY");
    }
    if (!env_api_key) {
        env_api_key = std::getenv("ANTHROPIC_API_KEY");
    }

    if (!env_api_key) {
        std::cerr << "❌ No API key found! Please set one of:" << std::endl;
        std::cerr << "   export QWEN_API_KEY=\"your-key\"" << std::endl;
        std::cerr << "   export OPENAI_API_KEY=\"your-key\"" << std::endl;
        std::cerr << "   export ANTHROPIC_API_KEY=\"your-key\"" << std::endl;
        std::cerr << std::endl;
        std::cerr << "Or use Ollama (local) by running:" << std::endl;
        std::cerr << "   " << argv[0] << " " << input << " ollama" << std::endl;
        return 1;
    }

    api_key = env_api_key;

    // Parse command line arguments for online providers
    if (argc > 2) {
        provider = argv[2];
    }
    if (argc > 3) {
        model = argv[3];
    }

    try {
        ReadmeChatBot bot(provider, api_key, model, false);

        // Determine if input is URL or file
        if (input.find("http") == 0) {
            if (!bot.loadReadmeFromUrl(input)) {
                return 1;
            }
        } else {
            if (!bot.loadReadmeFromFile(input)) {
                return 1;
            }
        }

        bot.startConversation();

    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
