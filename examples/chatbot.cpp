// Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This example demonstrates an interactive chatbot built with LLMEngine and is
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

class ChatBot {
private:
    std::unique_ptr<LLMEngine> engine_;
    std::string conversation_log_;
    bool debug_mode_;
    std::string mode_;
    
public:
    ChatBot(const std::string& provider_name, const std::string& api_key, 
            const std::string& model = "", bool debug = false, const std::string& mode = "chat") 
        : debug_mode_(debug), mode_(mode) {
        try {
            // Configure parameters optimized for chat interactions
            nlohmann::json chat_params = {
                {"temperature", 0.7},        // Balanced creativity
                {"max_tokens", 2000},         // Reasonable response length
                {"top_p", 0.9},              // Good balance of creativity and coherence
                {"frequency_penalty", 0.1},   // Slight penalty to avoid repetition
                {"presence_penalty", 0.0}     // No penalty for introducing new concepts
            };
            
            engine_ = std::make_unique<LLMEngine>(provider_name, api_key, model, 
                                                 chat_params, 24, debug);
            std::cout << "✓ ChatBot initialized with " << engine_->getProviderName() 
                      << " (" << (engine_->isOnlineProvider() ? "Online" : "Local") << ")" 
                      << " in " << mode_ << " mode" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "❌ Failed to initialize ChatBot: " << e.what() << std::endl;
            throw;
        }
    }
    
    void startConversation() {
        std::cout << "\n🤖 ChatBot started! Type 'quit', 'exit', or 'bye' to end the conversation." << std::endl;
        std::cout << "💡 Try: 'help' for commands, 'clear' to clear history, 'save' to save conversation" << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        
        std::string user_input;
        while (true) {
            std::cout << "\n👤 You: ";
            std::cout.flush(); // Ensure prompt is displayed
            
            if (!std::getline(std::cin, user_input)) {
                // Handle EOF or input error
                std::cout << "\n👋 Goodbye! Thanks for chatting!" << std::endl;
                break;
            }
            
            if (user_input.empty()) continue;
            
            // Handle special commands
            if (handleCommand(user_input)) {
                continue;
            }
            
            // Add to conversation log
            conversation_log_ += "User: " + user_input + "\n";
            
            try {
                // Get response from LLM
                auto result = engine_->analyze(user_input, nlohmann::json{}, "chat", mode_);
                std::string response = result[1];
                
                // Display response
                std::cout << "🤖 Bot: " << response << std::endl;
                
                // Add to conversation log
                conversation_log_ += "Bot: " + response + "\n\n";
                
            } catch (const std::exception& e) {
                std::cerr << "❌ Error getting response: " << e.what() << std::endl;
                std::cout << "🤖 Bot: I'm sorry, I encountered an error. Please try again." << std::endl;
            }
        }
    }
    
private:
    bool handleCommand(const std::string& input) {
        std::string cmd = input;
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
        
        if (cmd == "quit" || cmd == "exit" || cmd == "bye") {
            std::cout << "👋 Goodbye! Thanks for chatting!" << std::endl;
            exit(0);
        }
        else if (cmd == "help") {
            showHelp();
            return true;
        }
        else if (cmd == "clear") {
            conversation_log_.clear();
            std::cout << "🧹 Conversation history cleared!" << std::endl;
            return true;
        }
        else if (cmd == "save") {
            saveConversation();
            return true;
        }
        else if (cmd == "status") {
            showStatus();
            return true;
        }
        
        return false;
    }
    
    void showHelp() {
        std::cout << "\n📋 Available Commands:" << std::endl;
        std::cout << "  help     - Show this help message" << std::endl;
        std::cout << "  clear    - Clear conversation history" << std::endl;
        std::cout << "  save     - Save conversation to file" << std::endl;
        std::cout << "  status   - Show bot status and provider info" << std::endl;
        std::cout << "  quit/exit/bye - End conversation" << std::endl;
        std::cout << std::endl;
    }
    
    void showStatus() {
        std::cout << "\n📊 ChatBot Status:" << std::endl;
        std::cout << "  Provider: " << engine_->getProviderName() << std::endl;
        std::cout << "  Type: " << (engine_->isOnlineProvider() ? "Online API" : "Local") << std::endl;
        std::cout << "  Debug Mode: " << (debug_mode_ ? "Enabled" : "Disabled") << std::endl;
        std::cout << "  Conversation Length: " << conversation_log_.length() << " characters" << std::endl;
        std::cout << std::endl;
    }
    
    void saveConversation() {
        if (conversation_log_.empty()) {
            std::cout << "📝 No conversation to save!" << std::endl;
            return;
        }
        
        // Generate filename with timestamp
        auto now = std::time(nullptr);
        auto tm = *std::localtime(&now);
        std::stringstream ss;
        ss << "chatbot_conversation_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".txt";
        
        std::ofstream file(ss.str());
        if (file.is_open()) {
            file << "ChatBot Conversation Log\n";
            file << "Provider: " << engine_->getProviderName() << "\n";
            file << "Saved: " << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "\n";
            file << std::string(50, '=') << "\n\n";
            file << conversation_log_;
            file.close();
            std::cout << "💾 Conversation saved to: " << ss.str() << std::endl;
        } else {
            std::cerr << "❌ Failed to save conversation!" << std::endl;
        }
    }
};

void printWelcome() {
    std::cout << "🤖 LLMEngine ChatBot Example" << std::endl;
    std::cout << "============================" << std::endl;
    std::cout << "This example demonstrates interactive chat capabilities." << std::endl;
    std::cout << "Supports multiple AI providers: Qwen, OpenAI, Anthropic, Ollama" << std::endl;
    std::cout << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  ./chatbot ollama <model> [mode]" << std::endl;
    std::cout << "  ./chatbot <provider> <api_key> [model]" << std::endl;
    std::cout << std::endl;
    std::cout << "Modes for Ollama:" << std::endl;
    std::cout << "  chat     - Conversational chat (default)" << std::endl;
    std::cout << "  generate - Text completion/generation" << std::endl;
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    printWelcome();
    
    // Check if using Ollama first (no API key needed)
    if (argc > 1 && std::string(argv[1]) == "ollama") {
        try {
            std::string ollama_model = argc > 2 ? argv[2] : "llama2";
            std::string mode = argc > 3 ? argv[3] : "chat";
            
            if (mode != "chat" && mode != "generate") {
                std::cerr << "❌ Invalid mode. Use 'chat' or 'generate'" << std::endl;
                return 1;
            }
            
            ChatBot bot("ollama", "", ollama_model, false, mode);
            bot.startConversation();
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
        std::cerr << "   ./chatbot ollama" << std::endl;
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
        
        // Use API provider
        ChatBot bot(provider, api_key, model, false);
        bot.startConversation();
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
