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
#include <cctype>

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
                {"max_tokens", 32768},       // Reasonable response length
                {"top_p", 0.9},              // Good balance of creativity and coherence
                {"frequency_penalty", 0.2},   // Moderate penalty to avoid repetition
                {"presence_penalty", 0.1},     // Slight penalty for introducing new concepts
                {"think", true}              // Enable chain of thought reasoning
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
    
    std::string generateResponse(const std::string& problem, const std::string& other_bot_message = "", bool is_final_check = false) {
        try {
            std::string prompt = createPrompt(problem, other_bot_message, is_final_check);
            
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
    LLMEngine* getEngine() const { return engine_.get(); }
    
private:
    std::string createPrompt(const std::string& problem, const std::string& other_bot_message, bool is_final_check = false) {
        std::stringstream prompt;
        
        prompt << "You are " << name_ << ". " << personality_ << "\n\n";
        
        if (is_final_check) {
            prompt << "After reviewing our collaboration on: " << problem << "\n\n";
            prompt << "Based on our discussion, do you think we have provided a sufficient and practical solution? ";
            prompt << "Consider if we have covered the main aspects, provided actionable steps, and addressed the core problem. ";
            prompt << "If yes, respond with 'SOLUTION COMPLETE' followed by a brief summary. ";
            prompt << "If no, respond with 'NEED MORE DISCUSSION' and explain what's missing.";
            return prompt.str();
        }
        
        if (!other_bot_message.empty()) {
            prompt << "Your colleague just shared their analysis: \"" << other_bot_message << "\"\n\n";
            prompt << "Building on their work, contribute your expertise to solve this problem: " << problem << "\n\n";
            prompt << "Analyze their approach and add your perspective. ";
            prompt << "What additional insights, solutions, or improvements can you provide? ";
        } else {
            prompt << "You need to solve this problem: " << problem << "\n\n";
            prompt << "Apply your expertise to analyze the problem and propose initial solutions. ";
            prompt << "What is your approach and what do you think should be done first? ";
        }
        
        prompt << "Focus on practical problem-solving and actionable solutions. ";
        prompt << "Be specific and detailed in your analysis. ";
        prompt << "Consider implementation challenges, feasibility, and potential improvements. ";
        prompt << "Work collaboratively to build a comprehensive solution.";
        
        return prompt.str();
    }
};

class DualBotProblemSolver {
private:
    std::unique_ptr<Bot> bot1_;
    std::unique_ptr<Bot> bot2_;
    std::string problem_;
    std::string full_solution_log_;
    int max_turns_;
    bool debug_mode_;
    
public:
    DualBotProblemSolver(const std::string& problem, int max_turns = 10, bool debug = false) 
        : problem_(problem), max_turns_(max_turns), debug_mode_(debug) {
        full_solution_log_ = "Problem to Solve: " + problem_ + "\n";
        full_solution_log_ += std::string(50, '=') + "\n\n";
    }
    
    void initializeBots(const std::string& provider1, const std::string& api_key1, const std::string& model1,
                       const std::string& provider2, const std::string& api_key2, const std::string& model2) {
        // Create bots with different thinking styles
        std::vector<std::pair<std::string, std::string>> personalities = {
            {"Scientist", "You are a scientist. You think analytically and ask probing questions. You examine different angles and challenge assumptions to deepen understanding."},
            {"Engineer", "You are an engineer. You think creatively and innovatively. You explore unconventional ideas and brainstorm novel approaches to problems."},
            {"Optimizer", "You are an optimizer. You think practically and focus on real-world applications. You prefer concrete examples and actionable solutions."},
            {"Programmer", "You are a programmer. You think logically and focus on the practical implementation of ideas. You are a detail-oriented thinker and you are good at problem-solving."}
        };
        
        // Randomly assign personalities
        std::random_device rd;
        std::mt19937 gen(rd());
        std::shuffle(personalities.begin(), personalities.end(), gen);
        
        bot1_ = std::make_unique<Bot>(personalities[0].first, personalities[0].second, 
                                     provider1, api_key1, model1, debug_mode_);
        bot2_ = std::make_unique<Bot>(personalities[1].first, personalities[1].second, 
                                     provider2, api_key2, model2, debug_mode_);
        
        std::cout << "\n🤖 Problem-Solving Team Setup Complete!" << std::endl;
        std::cout << "Expert 1: " << bot1_->getName() << " (" << bot1_->getPersonality().substr(0, 50) << "...)" << std::endl;
        std::cout << "Expert 2: " << bot2_->getName() << " (" << bot2_->getPersonality().substr(0, 50) << "...)" << std::endl;
        std::cout << "Problem: " << problem_ << std::endl;
        std::cout << "Max Collaboration Rounds: " << max_turns_ << std::endl;
        std::cout << std::string(60, '=') << std::endl;
    }
    
    void startProblemSolving() {
        if (!bot1_ || !bot2_) {
            std::cerr << "❌ Problem-solving team not initialized! Please initialize bots first." << std::endl;
            return;
        }
        
        std::cout << "\n🎯 Starting collaborative problem-solving session" << std::endl;
        std::cout << "Experts: " << bot1_->getName() << " and " << bot2_->getName() << std::endl;
        std::cout << "Problem: " << problem_ << std::endl;
        std::cout << "The team will work together to solve this problem. Press Enter to start..." << std::endl;
        
        std::string input;
        std::getline(std::cin, input);
        if (input == "quit" || input == "exit") {
            return;
        }
        
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "PROBLEM-SOLVING SESSION BEGINS" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        std::string last_analysis = "";
        bool bot1_turn = true;
        
        for (int turn = 0; turn < max_turns_; ++turn) {
            std::string response;
            
            if (bot1_turn) {
                // Expert 1's turn
                std::cout << "\n🔬 " << bot1_->getName() << ": ";
                std::cout.flush();
                
                if (turn == 0) {
                    // First turn - initial analysis
                    response = bot1_->generateResponse(problem_);
                } else {
                    // Build on previous analysis
                    response = bot1_->generateResponse(problem_, last_analysis);
                }
                
                std::cout << response << std::endl;
                full_solution_log_ += bot1_->getName() + ": " + response + "\n";
                
            } else {
                // Expert 2's turn
                std::cout << "\n⚙️ " << bot2_->getName() << ": ";
                std::cout.flush();
                
                response = bot2_->generateResponse(problem_, last_analysis);
                std::cout << response << std::endl;
                full_solution_log_ += bot2_->getName() + ": " + response + "\n";
            }
            
            last_analysis = response;
            bot1_turn = !bot1_turn;
            
            // Check for completion after every 2 turns (both experts have spoken)
            if (turn > 0 && turn % 2 == 1) {
                if (checkForCompletion()) {
                    std::cout << "\n✅ Both experts agree the solution is complete!" << std::endl;
                    break;
                }
            }
            
            // Add a small delay for better readability
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "PROBLEM-SOLVING SESSION COMPLETED" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        // Generate final synthesized solution
        generateFinalSolution();
        
        // Offer to save the solution
        std::cout << "\n💾 Would you like to save this solution? (y/n): ";
        std::cout.flush();
        std::string save_choice;
        std::getline(std::cin, save_choice);
        
        if (save_choice == "y" || save_choice == "yes") {
            saveSolution();
        }
    }
    
private:
    bool checkForCompletion() {
        std::cout << "\n🤔 Checking if solution is complete..." << std::endl;
        
        try {
            // Ask both experts if they think the solution is complete
            std::string bot1_decision = bot1_->generateResponse(problem_, "", true);
            std::string bot2_decision = bot2_->generateResponse(problem_, "", true);
            
            std::cout << "\n🔬 " << bot1_->getName() << " decision: " << bot1_decision << std::endl;
            std::cout << "⚙️ " << bot2_->getName() << " decision: " << bot2_decision << std::endl;
            
            // Check if both experts agree (look for "SOLUTION COMPLETE" in their responses)
            bool bot1_agrees = bot1_decision.find("SOLUTION COMPLETE") != std::string::npos || 
                              bot1_decision.find("solution complete") != std::string::npos ||
                              bot1_decision.find("Solution Complete") != std::string::npos;
            bool bot2_agrees = bot2_decision.find("SOLUTION COMPLETE") != std::string::npos || 
                              bot2_decision.find("solution complete") != std::string::npos ||
                              bot2_decision.find("Solution Complete") != std::string::npos;
            
            // Additional check: if both are saying similar things (even if they say "NO"), 
            // use LLM to determine if they're essentially agreeing
            bool both_saying_similar = checkIfSimilarSolutionsWithLLM(bot1_decision, bot2_decision);
            
            // Add decisions to log
            full_solution_log_ += "\n--- COMPLETION CHECK ---\n";
            full_solution_log_ += bot1_->getName() + " decision: " + bot1_decision + "\n";
            full_solution_log_ += bot2_->getName() + " decision: " + bot2_decision + "\n";
            
            return bot1_agrees && bot2_agrees || both_saying_similar;
            
        } catch (const std::exception& e) {
            std::cerr << "❌ Error checking completion: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool checkIfSimilarSolutionsWithLLM(const std::string& decision1, const std::string& decision2) {
        try {
            // Create a comparison prompt
            std::stringstream comparison_prompt;
            comparison_prompt << "You are an expert mediator analyzing two responses about problem-solving completion.\n\n";
            comparison_prompt << "Expert 1 says: \"" << decision1 << "\"\n\n";
            comparison_prompt << "Expert 2 says: \"" << decision2 << "\"\n\n";
            comparison_prompt << "Both experts seem to be saying 'NO' or 'need more discussion', but analyze if they are actually ";
            comparison_prompt << "saying similar things about the solution. Look for:\n";
            comparison_prompt << "1. Do they mention the same key concepts or solutions?\n";
            comparison_prompt << "2. Are they both satisfied with the technical approach?\n";
            comparison_prompt << "3. Are they just being cautious rather than disagreeing?\n\n";
            comparison_prompt << "Respond with exactly 'SIMILAR' if they are essentially saying the same thing about the solution, ";
            comparison_prompt << "or 'DIFFERENT' if they have fundamentally different approaches or missing elements.";
            
            // Use the first bot's engine to make the comparison
            auto result = bot1_->getEngine()->analyze(comparison_prompt.str(), nlohmann::json{}, "chat", "chat");
            std::string comparison_result = result[1];
            
            // Check if the LLM determined they are similar
            bool is_similar = comparison_result.find("SIMILAR") != std::string::npos;
            
            if (is_similar) {
                std::cout << "\n🤖 LLM Analysis: Both experts are saying similar things about the solution!" << std::endl;
            }
            
            return is_similar;
            
        } catch (const std::exception& e) {
            std::cerr << "❌ Error in LLM comparison: " << e.what() << std::endl;
            return false;
        }
    }
    
    void generateFinalSolution() {
        std::cout << "\n🧠 SYNTHESIZING FINAL SOLUTION" << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        
        try {
            // Create a synthesis prompt using the first bot's engine
            std::stringstream synthesis_prompt;
            synthesis_prompt << "You are a senior technical consultant tasked with synthesizing a comprehensive solution.\n\n";
            synthesis_prompt << "Problem: " << problem_ << "\n\n";
            synthesis_prompt << "Collaboration History:\n";
            synthesis_prompt << full_solution_log_ << "\n\n";
            synthesis_prompt << "Based on the above collaboration between " << bot1_->getName() << " and " << bot2_->getName() << ", ";
            synthesis_prompt << "provide a single, comprehensive, actionable solution that:\n";
            synthesis_prompt << "1. Integrates the best ideas from both experts\n";
            synthesis_prompt << "2. Provides a clear step-by-step implementation plan\n";
            synthesis_prompt << "3. Addresses potential challenges and considerations\n";
            synthesis_prompt << "4. Includes specific tools, technologies, and methodologies\n";
            synthesis_prompt << "5. Prioritizes the most impactful solutions\n\n";
            synthesis_prompt << "Format your response as a structured solution with clear sections and actionable steps.";
            
            // Use the first bot's engine to generate the synthesis
            auto result = bot1_->getEngine()->analyze(synthesis_prompt.str(), nlohmann::json{}, "chat", "chat");
            std::string final_solution = result[1];
            
            std::cout << "\n📋 COMPREHENSIVE SOLUTION:" << std::endl;
            std::cout << std::string(50, '-') << std::endl;
            std::cout << final_solution << std::endl;
            std::cout << std::string(50, '-') << std::endl;
            
            // Add the final solution to the log
            full_solution_log_ += "\n" + std::string(50, '=') + "\n";
            full_solution_log_ += "FINAL SYNTHESIZED SOLUTION\n";
            full_solution_log_ += std::string(50, '=') + "\n";
            full_solution_log_ += final_solution + "\n";
            
        } catch (const std::exception& e) {
            std::cerr << "❌ Error generating final solution: " << e.what() << std::endl;
            std::cout << "📋 Unable to synthesize final solution, but the collaboration history above contains all the insights." << std::endl;
        }
    }
    
    void saveSolution() {
        // Generate filename with timestamp
        auto now = std::time(nullptr);
        auto tm = *std::localtime(&now);
        std::stringstream ss;
        ss << "problem_solution_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".txt";
        
        std::ofstream file(ss.str());
        if (file.is_open()) {
            file << "Collaborative Problem-Solving Session\n";
            file << "Problem: " << problem_ << "\n";
            file << "Expert 1: " << bot1_->getName() << " (" << bot1_->getPersonality() << ")\n";
            file << "Expert 2: " << bot2_->getName() << " (" << bot2_->getPersonality() << ")\n";
            file << "Saved: " << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "\n";
            file << std::string(50, '=') << "\n\n";
            file << full_solution_log_;
            file.close();
            std::cout << "💾 Solution saved to: " << ss.str() << std::endl;
        } else {
            std::cerr << "❌ Failed to save solution!" << std::endl;
        }
    }
};

void printWelcome() {
    std::cout << "🔬⚙️ LLMEngine Dual Bot Problem Solver" << std::endl;
    std::cout << "=====================================" << std::endl;
    std::cout << "This example creates two AI experts that collaborate to solve problems." << std::endl;
    std::cout << "Each expert has specialized knowledge and approaches problems differently." << std::endl;
    std::cout << "Supports multiple AI providers: Qwen, OpenAI, Anthropic, Ollama" << std::endl;
    std::cout << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  ./dual_bot_chat \"<problem description>\" [provider1] [model1] [provider2] [model2]" << std::endl;
    std::cout << "  ./dual_bot_chat \"<problem description>\" ollama [model1] [model2]" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  ./dual_bot_chat \"How to optimize database performance for a web app\" ollama qwen3:1.7b qwen3:1.7b" << std::endl;
    std::cout << "  ./dual_bot_chat \"Design a scalable microservices architecture\" qwen qwen-flash qwen qwen-max" << std::endl;
    std::cout << "  ./dual_bot_chat \"Reduce energy consumption in data centers\" ollama qwen3:4b qwen3:1.7b" << std::endl;
    std::cout << std::endl;
    std::cout << "The experts will collaborate automatically with a 1-second delay between contributions." << std::endl;
    std::cout << "You can save the complete solution at the end if desired." << std::endl;
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    printWelcome();
    
    if (argc < 2) {
        std::cerr << "❌ Please provide a problem to solve!" << std::endl;
        std::cerr << "Usage: " << argv[0] << " \"<problem description>\" [provider1] [model1] [provider2] [model2]" << std::endl;
        return 1;
    }
    
    std::string problem = argv[1];
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
            
            DualBotProblemSolver solver(problem, 50, false);
            solver.initializeBots("ollama", "", model1, "ollama", "", model2);
            solver.startProblemSolving();
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
        std::cerr << "   " << argv[0] << " \"" << problem << "\" ollama" << std::endl;
        return 1;
    }
    
    api_key1 = api_key2 = env_api_key;
    
    // Parse command line arguments for online providers
    if (argc > 2) provider1 = argv[2];
    if (argc > 3) model1 = argv[3];
    if (argc > 4) provider2 = argv[4];
    if (argc > 5) model2 = argv[5];
    
    try {
        DualBotProblemSolver solver(problem, 50, false);
        solver.initializeBots(provider1, api_key1, model1, provider2, api_key2, model2);
        solver.startProblemSolving();
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
