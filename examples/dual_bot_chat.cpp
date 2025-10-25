#include "LLMEngine.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>
#include <chrono>
#include <random>
#include <algorithm>

class Bot {
private:
    std::unique_ptr<LLMEngine> engine_;
    std::string name_;
    std::string personality_;
    std::string conversation_history_;
    bool debug_mode_;
    
public:
    Bot(const std::string& name, const std::string& personality, 
        const std::string& provider_name, const std::string& api_key, 
        const std::string& model = "", bool debug = false) 
        : name_(name), personality_(personality), debug_mode_(debug) {
        try {
            // Configure parameters optimized for conversational interactions
            nlohmann::json chat_params = {
                {"temperature", 0.8},        // Higher creativity for diverse responses
                {"max_tokens", 300},          // Shorter responses for better conversation flow
                {"top_p", 0.9},              // Good balance of creativity and coherence
                {"frequency_penalty", 0.2},   // Moderate penalty to avoid repetition
                {"presence_penalty", 0.1}     // Slight penalty for introducing new concepts
            };
            
            engine_ = std::make_unique<LLMEngine>(provider_name, api_key, model, 
                                                 chat_params, 24, debug);
            std::cout << "✓ " << name_ << " initialized with " << engine_->getProviderName() 
                      << " (" << (engine_->isOnlineProvider() ? "Online" : "Local") << ")" 
                      << " - Personality: " << personality_ << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "❌ Failed to initialize " << name_ << ": " << e.what() << std::endl;
            throw;
        }
    }
    
    std::string generateResponse(const std::string& topic, const std::string& other_bot_message = "") {
        try {
            std::string prompt = createPrompt(topic, other_bot_message);
            
            auto result = engine_->analyze(prompt, nlohmann::json{}, "chat", "chat");
            std::string response = result[1];
            
            // Add to conversation history
            conversation_history_ += name_ + ": " + response + "\n";
            
            return response;
            
        } catch (const std::exception& e) {
            std::cerr << "❌ Error generating response for " << name_ << ": " << e.what() << std::endl;
            return "I'm sorry, I encountered an error and couldn't respond properly.";
        }
    }
    
    std::string getName() const { return name_; }
    std::string getPersonality() const { return personality_; }
    std::string getConversationHistory() const { return conversation_history_; }
    
private:
    std::string createPrompt(const std::string& topic, const std::string& other_bot_message) {
        std::stringstream prompt;
        
        prompt << "You are " << name_ << ", a chatbot with the following personality: " << personality_ << "\n\n";
        
        if (!other_bot_message.empty()) {
            prompt << "Another bot just said: \"" << other_bot_message << "\"\n\n";
            prompt << "Respond to their message while staying in character. ";
        } else {
            prompt << "Start a conversation about: " << topic << " ";
        }
        
        prompt << "Keep your response conversational, engaging, and true to your personality. ";
        prompt << "Make it sound natural and human-like. ";
        prompt << "Keep responses concise (1-3 sentences) to maintain good conversation flow. ";
        prompt << "Don't repeat what was already said unless adding new insights.";
        
        return prompt.str();
    }
};

class DualBotConversation {
private:
    std::unique_ptr<Bot> bot1_;
    std::unique_ptr<Bot> bot2_;
    std::string topic_;
    std::string full_conversation_log_;
    int max_turns_;
    bool debug_mode_;
    
public:
    DualBotConversation(const std::string& topic, int max_turns = 10, bool debug = false) 
        : topic_(topic), max_turns_(max_turns), debug_mode_(debug) {
        full_conversation_log_ = "Conversation Topic: " + topic_ + "\n";
        full_conversation_log_ += std::string(50, '=') + "\n\n";
    }
    
    void initializeBots(const std::string& provider1, const std::string& api_key1, const std::string& model1,
                       const std::string& provider2, const std::string& api_key2, const std::string& model2) {
        // Create bots with different personalities
        std::vector<std::pair<std::string, std::string>> personalities = {
            {"Alice", "You are curious, analytical, and tend to ask thoughtful questions. You like to explore different perspectives and challenge ideas constructively."},
            {"Bob", "You are enthusiastic, creative, and tend to think outside the box. You enjoy brainstorming and coming up with innovative solutions."},
            {"Charlie", "You are practical, detail-oriented, and focus on real-world applications. You prefer concrete examples and actionable insights."},
            {"Diana", "You are philosophical, reflective, and enjoy exploring deeper meanings. You tend to connect ideas to broader concepts and principles."}
        };
        
        // Randomly assign personalities
        std::random_device rd;
        std::mt19937 gen(rd());
        std::shuffle(personalities.begin(), personalities.end(), gen);
        
        bot1_ = std::make_unique<Bot>(personalities[0].first, personalities[0].second, 
                                     provider1, api_key1, model1, debug_mode_);
        bot2_ = std::make_unique<Bot>(personalities[1].first, personalities[1].second, 
                                     provider2, api_key2, model2, debug_mode_);
        
        std::cout << "\n🤖 Bot Setup Complete!" << std::endl;
        std::cout << "Bot 1: " << bot1_->getName() << " (" << bot1_->getPersonality().substr(0, 50) << "...)" << std::endl;
        std::cout << "Bot 2: " << bot2_->getName() << " (" << bot2_->getPersonality().substr(0, 50) << "...)" << std::endl;
        std::cout << "Topic: " << topic_ << std::endl;
        std::cout << "Max Turns: " << max_turns_ << std::endl;
        std::cout << std::string(60, '=') << std::endl;
    }
    
    void startConversation() {
        if (!bot1_ || !bot2_) {
            std::cerr << "❌ Bots not initialized! Please initialize bots first." << std::endl;
            return;
        }
        
    std::cout << "\n🎬 Starting conversation between " << bot1_->getName() 
              << " and " << bot2_->getName() << " about: " << topic_ << std::endl;
    std::cout << "The conversation will flow automatically. Press Enter to start..." << std::endl;
        
        std::string input;
        std::getline(std::cin, input);
        if (input == "quit" || input == "exit") {
            return;
        }
        
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "CONVERSATION BEGINS" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        std::string last_message = "";
        bool bot1_turn = true;
        
        for (int turn = 0; turn < max_turns_; ++turn) {
            std::string response;
            
            if (bot1_turn) {
                // Bot 1's turn
                std::cout << "\n🤖 " << bot1_->getName() << ": ";
                std::cout.flush();
                
                if (turn == 0) {
                    // First turn - start the conversation
                    response = bot1_->generateResponse(topic_);
                } else {
                    // Respond to the other bot
                    response = bot1_->generateResponse(topic_, last_message);
                }
                
                std::cout << response << std::endl;
                full_conversation_log_ += bot1_->getName() + ": " + response + "\n";
                
            } else {
                // Bot 2's turn
                std::cout << "\n🤖 " << bot2_->getName() << ": ";
                std::cout.flush();
                
                response = bot2_->generateResponse(topic_, last_message);
                std::cout << response << std::endl;
                full_conversation_log_ += bot2_->getName() + ": " + response + "\n";
            }
            
            last_message = response;
            bot1_turn = !bot1_turn;
            
            // Add a small delay for better readability
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "CONVERSATION ENDED" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        // Offer to save the conversation
        std::cout << "\n💾 Would you like to save this conversation? (y/n): ";
        std::cout.flush();
        std::string save_choice;
        std::getline(std::cin, save_choice);
        
        if (save_choice == "y" || save_choice == "yes") {
            saveConversation();
        }
    }
    
private:
    void saveConversation() {
        // Generate filename with timestamp
        auto now = std::time(nullptr);
        auto tm = *std::localtime(&now);
        std::stringstream ss;
        ss << "dual_bot_conversation_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".txt";
        
        std::ofstream file(ss.str());
        if (file.is_open()) {
            file << "Dual Bot Conversation Log\n";
            file << "Topic: " << topic_ << "\n";
            file << "Bot 1: " << bot1_->getName() << " (" << bot1_->getPersonality() << ")\n";
            file << "Bot 2: " << bot2_->getName() << " (" << bot2_->getPersonality() << ")\n";
            file << "Saved: " << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "\n";
            file << std::string(50, '=') << "\n\n";
            file << full_conversation_log_;
            file.close();
            std::cout << "💾 Conversation saved to: " << ss.str() << std::endl;
        } else {
            std::cerr << "❌ Failed to save conversation!" << std::endl;
        }
    }
};

void printWelcome() {
    std::cout << "🤖🤖 LLMEngine Dual Bot Chat Example" << std::endl;
    std::cout << "====================================" << std::endl;
    std::cout << "This example creates two AI bots that talk to each other about a given topic." << std::endl;
    std::cout << "Each bot has a unique personality and will engage in natural conversation." << std::endl;
    std::cout << "Supports multiple AI providers: Qwen, OpenAI, Anthropic, Ollama" << std::endl;
    std::cout << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  ./dual_bot_chat <topic> [provider1] [model1] [provider2] [model2]" << std::endl;
    std::cout << "  ./dual_bot_chat <topic> ollama [model1] [model2]" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  ./dual_bot_chat \"artificial intelligence\" ollama llama2 llama2" << std::endl;
    std::cout << "  ./dual_bot_chat \"climate change\" qwen qwen-flash qwen qwen-max" << std::endl;
    std::cout << "  ./dual_bot_chat \"space exploration\" ollama mistral mistral" << std::endl;
    std::cout << std::endl;
    std::cout << "The conversation will flow automatically with a 1-second delay between turns." << std::endl;
    std::cout << "You can save the conversation at the end if desired." << std::endl;
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    printWelcome();
    
    if (argc < 2) {
        std::cerr << "❌ Please provide a conversation topic!" << std::endl;
        std::cerr << "Usage: " << argv[0] << " <topic> [provider1] [model1] [provider2] [model2]" << std::endl;
        return 1;
    }
    
    std::string topic = argv[1];
    std::string provider1 = "qwen";
    std::string model1 = "qwen-flash";
    std::string provider2 = "qwen";
    std::string model2 = "qwen-flash";
    std::string api_key1, api_key2;
    
    // Check if using Ollama
    if (argc > 2 && std::string(argv[2]) == "ollama") {
        try {
            model1 = argc > 3 ? argv[3] : "llama2";
            model2 = argc > 4 ? argv[4] : "llama2";
            
            DualBotConversation conversation(topic, 10, false);
            conversation.initializeBots("ollama", "", model1, "ollama", "", model2);
            conversation.startConversation();
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
        std::cerr << "   " << argv[0] << " \"" << topic << "\" ollama" << std::endl;
        return 1;
    }
    
    api_key1 = api_key2 = env_api_key;
    
    // Parse command line arguments for online providers
    if (argc > 2) provider1 = argv[2];
    if (argc > 3) model1 = argv[3];
    if (argc > 4) provider2 = argv[4];
    if (argc > 5) model2 = argv[5];
    
    try {
        DualBotConversation conversation(topic, 10, false);
        conversation.initializeBots(provider1, api_key1, model1, provider2, api_key2, model2);
        conversation.startConversation();
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
