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
    
    std::string generateResponse(const std::string& problem, const std::vector<std::string>& previous_messages = {}, bool is_final_check = false) {
        try {
            std::string prompt = createPrompt(problem, previous_messages, is_final_check);
            
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
    std::string createPrompt(const std::string& problem, const std::vector<std::string>& previous_messages, bool is_final_check = false) {
        std::stringstream prompt;
        
        prompt << "You are " << name_ << ". " << personality_ << "\n\n";
        
        if (is_final_check) {
            prompt << "After reviewing our collaboration on: " << problem << "\n\n";
            prompt << "DEMOCRATIC VOTING: Based on our discussion, do you think we have provided a sufficient and practical solution? ";
            prompt << "Consider if we have covered the main aspects, provided actionable steps, and addressed the core problem. ";
            prompt << "Your vote will be counted democratically - if 66% or more experts vote 'SOLUTION COMPLETE', the solution will be accepted. ";
            prompt << "If yes, respond with 'SOLUTION COMPLETE' followed by a brief summary. ";
            prompt << "If no, respond with 'NEED MORE DISCUSSION' and explain what's missing.";
            return prompt.str();
        }
        
        if (!previous_messages.empty()) {
            prompt << "Your colleagues have shared their analyses:\n";
            for (size_t i = 0; i < previous_messages.size(); ++i) {
                prompt << "Colleague " << (i + 1) << ": \"" << previous_messages[i] << "\"\n";
            }
            prompt << "\nBuilding on their work, contribute your expertise to solve this problem: " << problem << "\n\n";
            prompt << "Analyze their approaches and add your perspective. ";
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

class MultiBotProblemSolver {
private:
    std::vector<std::unique_ptr<Bot>> bots_;
    std::string problem_;
    std::string full_solution_log_;
    int max_turns_;
    bool debug_mode_;
    
    std::string formatExpertName(const Bot* bot) const {
        std::string name = bot->getName();
        
        // Check if all bots use the same model
        bool all_same_model = true;
        if (!bots_.empty()) {
            std::string first_model = bots_[0]->getEngine()->getModelName();
            for (const auto& bot : bots_) {
                if (bot->getEngine()->getModelName() != first_model) {
                    all_same_model = false;
                    break;
                }
            }
        }
        
        // Only show model name if the models are different
        if (!all_same_model) {
            std::string model = bot->getEngine()->getModelName();
            if (!model.empty()) {
                name += " (" + model + ")";
            }
        }
        
        return name;
    }
    
public:
    MultiBotProblemSolver(const std::string& problem, int max_turns = 10, bool debug = false) 
        : problem_(problem), max_turns_(max_turns), debug_mode_(debug) {
        full_solution_log_ = "Problem to Solve: " + problem_ + "\n";
        full_solution_log_ += std::string(50, '=') + "\n\n";
    }
    
    void initializeBots(const std::vector<std::tuple<std::string, std::string, std::string>>& bot_configs) {
        if (bot_configs.empty() || bot_configs.size() > 4) {
            throw std::invalid_argument("Must provide 1-4 bot configurations");
        }
        
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
        
        bots_.clear();
        for (size_t i = 0; i < bot_configs.size(); ++i) {
            const auto& config = bot_configs[i];
            std::string provider = std::get<0>(config);
            std::string api_key = std::get<1>(config);
            std::string model = std::get<2>(config);
            
            bots_.push_back(std::make_unique<Bot>(
                personalities[i].first, 
                personalities[i].second, 
                provider, 
                api_key, 
                model, 
                debug_mode_
            ));
        }
        
        std::cout << "\n🤖 Problem-Solving Team Setup Complete!" << std::endl;
        for (size_t i = 0; i < bots_.size(); ++i) {
            std::cout << "Expert " << (i + 1) << ": " << formatExpertName(bots_[i].get()) 
                      << " (" << bots_[i]->getPersonality().substr(0, 50) << "...)" << std::endl;
        }
        std::cout << "Problem: " << problem_ << std::endl;
        std::cout << "Max Collaboration Rounds: " << max_turns_ << std::endl;
        std::cout << std::string(60, '=') << std::endl;
    }
    
    void startProblemSolving() {
        if (bots_.empty()) {
            std::cerr << "❌ Problem-solving team not initialized! Please initialize bots first." << std::endl;
            return;
        }
        
        std::cout << "\n🎯 Starting collaborative problem-solving session" << std::endl;
        std::cout << "Experts: ";
        for (size_t i = 0; i < bots_.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << formatExpertName(bots_[i].get());
        }
        std::cout << std::endl;
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
        
        std::vector<std::string> conversation_history;
        
        for (int turn = 0; turn < max_turns_; ++turn) {
            int bot_index = turn % bots_.size();
            Bot* current_bot = bots_[bot_index].get();
            
            // Choose appropriate emoji based on bot personality
            std::string emoji = "🤖";
            std::string bot_name = current_bot->getName();
            if (bot_name == "Scientist") emoji = "🔬";
            else if (bot_name == "Engineer") emoji = "⚙️";
            else if (bot_name == "Optimizer") emoji = "📊";
            else if (bot_name == "Programmer") emoji = "💻";
            
            std::cout << "\n" << emoji << " " << formatExpertName(current_bot) << ": ";
            std::cout.flush();
            
            std::string response = current_bot->generateResponse(problem_, conversation_history);
            std::cout << response << std::endl;
            
            conversation_history.push_back(response);
            full_solution_log_ += current_bot->getName() + ": " + response + "\n";
            
            // Check for completion after every full round (all bots have spoken)
            if (turn > 0 && (turn + 1) % bots_.size() == 0) {
                if (checkForCompletion()) {
                    std::cout << "\n✅ Democratic consensus reached! Solution accepted by majority vote!" << std::endl;
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
        std::cout << "\n🗳️ Democratic voting: Checking if solution is complete..." << std::endl;
        
        try {
            // Ask all experts if they think the solution is complete
            std::vector<std::string> decisions;
            for (const auto& bot : bots_) {
                std::string decision = bot->generateResponse(problem_, {}, true);
                decisions.push_back(decision);
                std::cout << "\n" << formatExpertName(bot.get()) << " decision: " << decision << std::endl;
            }
            
            // Count votes for "SOLUTION COMPLETE"
            int solution_complete_votes = 0;
            int total_votes = bots_.size();
            
            for (const auto& decision : decisions) {
                bool votes_complete = decision.find("SOLUTION COMPLETE") != std::string::npos || 
                                      decision.find("solution complete") != std::string::npos ||
                                      decision.find("Solution Complete") != std::string::npos;
                if (votes_complete) {
                    solution_complete_votes++;
                }
            }
            
            // Calculate percentage
            double completion_percentage = (double)solution_complete_votes / total_votes * 100.0;
            
            std::cout << "\n📊 Voting Results:" << std::endl;
            std::cout << "   SOLUTION COMPLETE votes: " << solution_complete_votes << "/" << total_votes << std::endl;
            std::cout << "   Completion percentage: " << std::fixed << std::setprecision(1) << completion_percentage << "%" << std::endl;
            std::cout << "   Required threshold: 66.0%" << std::endl;
            
            // Check if we meet the 66% threshold
            bool meets_threshold = completion_percentage >= 66.0;
            
            if (meets_threshold) {
                std::cout << "✅ Democratic decision: Solution accepted! (" << completion_percentage << "% ≥ 66%)" << std::endl;
            } else {
                std::cout << "❌ Democratic decision: Solution needs more work (" << completion_percentage << "% < 66%)" << std::endl;
            }
            
            // Add decisions to log
            full_solution_log_ += "\n--- DEMOCRATIC VOTING RESULTS ---\n";
            full_solution_log_ += "SOLUTION COMPLETE votes: " + std::to_string(solution_complete_votes) + "/" + std::to_string(total_votes) + "\n";
            full_solution_log_ += "Completion percentage: " + std::to_string(completion_percentage) + "%\n";
            full_solution_log_ += "Required threshold: 66.0%\n";
            full_solution_log_ += "Decision: " + std::string(meets_threshold ? "ACCEPTED" : "NEEDS MORE WORK") + "\n\n";
            
            for (size_t i = 0; i < bots_.size(); ++i) {
                full_solution_log_ += bots_[i]->getName() + " vote: " + decisions[i] + "\n";
            }
            
            return meets_threshold;
            
        } catch (const std::exception& e) {
            std::cerr << "❌ Error checking completion: " << e.what() << std::endl;
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
            synthesis_prompt << "Based on the above collaboration between ";
            for (size_t i = 0; i < bots_.size(); ++i) {
                if (i > 0) synthesis_prompt << ", ";
                synthesis_prompt << formatExpertName(bots_[i].get());
            }
            synthesis_prompt << ", provide a single, comprehensive, actionable solution that:\n";
            synthesis_prompt << "1. Integrates the best ideas from all experts\n";
            synthesis_prompt << "2. Provides a clear step-by-step implementation plan\n";
            synthesis_prompt << "3. Addresses potential challenges and considerations\n";
            synthesis_prompt << "4. Includes specific tools, technologies, and methodologies\n";
            synthesis_prompt << "5. Prioritizes the most impactful solutions\n\n";
            synthesis_prompt << "Format your response as a structured solution with clear sections and actionable steps.";
            
            // Use the first bot's engine to generate the synthesis
            auto result = bots_[0]->getEngine()->analyze(synthesis_prompt.str(), nlohmann::json{}, "chat", "chat");
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
            for (size_t i = 0; i < bots_.size(); ++i) {
                file << "Expert " << (i + 1) << ": " << formatExpertName(bots_[i].get()) 
                     << " (" << bots_[i]->getPersonality() << ")\n";
            }
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
    std::cout << "🤖 LLMEngine Multi-Bot Problem Solver" << std::endl;
    std::cout << "=====================================" << std::endl;
    std::cout << "This example creates 1-4 AI experts that collaborate to solve problems." << std::endl;
    std::cout << "Each expert has specialized knowledge and approaches problems differently." << std::endl;
    std::cout << "🗳️ Uses democratic voting: Solutions are accepted when 66%+ experts vote 'SOLUTION COMPLETE'." << std::endl;
    std::cout << "Supports multiple AI providers: Qwen, OpenAI, Anthropic, Ollama" << std::endl;
    std::cout << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  ./multi_bot_solver \"<problem description>\" [num_bots] [provider1] [model1] [provider2] [model2] ..." << std::endl;
    std::cout << "  ./multi_bot_solver \"<problem description>\" [num_bots] ollama [model1] [model2] ..." << std::endl;
    std::cout << "  ./multi_bot_solver \"<problem description>\" [num_bots] ollama [single_model]  # Uses same model for all bots" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  ./multi_bot_solver \"How to optimize database performance\" 2 ollama qwen3:1.7b" << std::endl;
    std::cout << "  ./multi_bot_solver \"Design a scalable architecture\" 3 qwen qwen-flash qwen qwen-max qwen qwen-plus" << std::endl;
    std::cout << "  ./multi_bot_solver \"Reduce energy consumption\" 4 ollama qwen3:4b  # All 4 bots use qwen3:4b" << std::endl;
    std::cout << "  ./multi_bot_solver \"Solve complex algorithm problem\" 1 ollama qwen3:4b" << std::endl;
    std::cout << std::endl;
    std::cout << "The experts will collaborate automatically with a 1-second delay between contributions." << std::endl;
    std::cout << "You can save the complete solution at the end if desired." << std::endl;
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    printWelcome();
    
    if (argc < 3) {
        std::cerr << "❌ Please provide a problem to solve and number of bots!" << std::endl;
        std::cerr << "Usage: " << argv[0] << " \"<problem description>\" [num_bots] [provider1] [model1] [provider2] [model2] ..." << std::endl;
        std::cerr << "       " << argv[0] << " \"<problem description>\" [num_bots] ollama [model1] [model2] ..." << std::endl;
        return 1;
    }
    
    std::string problem = argv[1];
    
    int num_bots;
    try {
        num_bots = std::stoi(argv[2]);
    } catch (const std::exception& e) {
        std::cerr << "❌ Invalid number of bots: " << argv[2] << std::endl;
        std::cerr << "Number of bots must be a valid integer between 1 and 4." << std::endl;
        return 1;
    }
    
    if (num_bots < 1 || num_bots > 4) {
        std::cerr << "❌ Number of bots must be between 1 and 4!" << std::endl;
        return 1;
    }
    
    // Check if using Ollama
    if (argc > 3 && std::string(argv[3]) == "ollama") {
        try {
            std::vector<std::tuple<std::string, std::string, std::string>> bot_configs;
            
            // If only one model is provided, use it for all bots
            std::string model = argc > 4 ? argv[4] : "llama2";
            for (int i = 0; i < num_bots; ++i) {
                bot_configs.push_back(std::make_tuple("ollama", "", model));
            }
            
            MultiBotProblemSolver solver(problem, 50, false);
            solver.initializeBots(bot_configs);
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
        std::cerr << "   " << argv[0] << " \"" << problem << "\" " << num_bots << " ollama" << std::endl;
        return 1;
    }
    
    std::string api_key = env_api_key;
    
    // Parse command line arguments for online providers
    std::vector<std::tuple<std::string, std::string, std::string>> bot_configs;
    
    for (int i = 0; i < num_bots; ++i) {
        std::string provider = argc > (3 + i * 2) ? argv[3 + i * 2] : "qwen";
        std::string model = argc > (4 + i * 2) ? argv[4 + i * 2] : "qwen-flash";
        bot_configs.push_back(std::make_tuple(provider, api_key, model));
    }
    
    try {
        MultiBotProblemSolver solver(problem, 50, false);
        solver.initializeBots(bot_configs);
        solver.startProblemSolving();
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
