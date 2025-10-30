# LLMEngine API Integration - Quick Start Guide

## 🎉 What's New

The LLMEngine library now supports **online AI model APIs** including:
- ✅ **Qwen (DashScope)** - Alibaba's powerful language models
- ✅ **OpenAI** - GPT models
- ✅ **Anthropic Claude** - Claude models
- ✅ **Ollama** - Local models (enhanced support)

## 🚀 Quick Start

See also: `docs/CONFIGURATION.md`, `docs/PROVIDERS.md`, and `docs/API_REFERENCE.md`.

### 1. Using Qwen API (Recommended for Testing)

```cpp
#include "LLMEngine.hpp"

int main() {
    // Get your API key from environment
    const char* api_key = std::getenv("QWEN_API_KEY");
    
    // Create engine with Qwen
    LLMEngine engine(::LLMEngineAPI::ProviderType::QWEN, 
                     api_key, 
                     "qwen-flash");
    
    // Make a request
    std::string prompt = "Explain quantum computing in simple terms:";
    nlohmann::json input = {};
    auto result = engine.analyze(prompt, input, "test");
    
    // Print response
    std::cout << "Response: " << result[1] << std::endl;
    
    return 0;
}
```

### 2. Get Your Qwen API Key

1. Visit: https://dashscope.console.aliyuncs.com/
2. Sign up or log in
3. Navigate to **API Keys** section
4. Click **Create API Key**
5. Copy your key and set it:
   ```bash
   export QWEN_API_KEY="sk-your-api-key-here"
   ```

### 3. Build and Run

```bash
# Rebuild the library
cd build
cmake ..
make -j20

# Create a test program
cat > test_qwen.cpp << 'EOF'
#include <iostream>
#include <cstdlib>
#include "LLMEngine.hpp"

int main() {
    const char* api_key = std::getenv("QWEN_API_KEY");
    if (!api_key) {
        std::cerr << "Please set QWEN_API_KEY environment variable" << std::endl;
        return 1;
    }
    
    try {
        LLMEngine engine(::LLMEngineAPI::ProviderType::QWEN, api_key, "qwen-flash");
        
        std::string prompt = "What is 2+2? Answer with just the number.";
        nlohmann::json input = {};
        auto result = engine.analyze(prompt, input, "math_test");
        
        std::cout << "Answer: " << result[1] << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
EOF

# Compile
g++ -std=c++17 test_qwen.cpp -o test_qwen \
    -I../src \
    -L. -lLLMEngine \
    $(pkg-config --cflags --libs libcpr nlohmann_json)

# Run
export QWEN_API_KEY="your-key"
./test_qwen
```

## 📖 Usage Examples

### Using Different Providers

```cpp
// Qwen
LLMEngine qwen(::LLMEngineAPI::ProviderType::QWEN, api_key, "qwen-flash");

// OpenAI
LLMEngine openai(::LLMEngineAPI::ProviderType::OPENAI, api_key, "gpt-3.5-turbo");

// Anthropic Claude
LLMEngine claude(::LLMEngineAPI::ProviderType::ANTHROPIC, api_key, "claude-3-sonnet");

// Ollama (local)
LLMEngine ollama("http://localhost:11434", "llama2");
```

### Using Provider Names (String-based)

```cpp
// Load from config file
LLMEngine engine("qwen", api_key, "qwen-flash");

// Or with default model from config
LLMEngine engine2("qwen", api_key);
```

### With Custom Parameters

```cpp
nlohmann::json params = {
    {"temperature", 0.7},
    {"max_tokens", 2000},
    {"top_p", 0.9}
};

LLMEngine engine(::LLMEngineAPI::ProviderType::QWEN, 
                 api_key, 
                 "qwen-plus", 
                 params);
```

### Check Provider Information

```cpp
LLMEngine engine(::LLMEngineAPI::ProviderType::QWEN, api_key, "qwen-flash");

std::cout << "Provider: " << engine.getProviderName() << std::endl;
std::cout << "Is online: " << (engine.isOnlineProvider() ? "Yes" : "No") << std::endl;
```

## 🔧 Configuration

### API Configuration File

Edit `config/api_config.json` to customize:
- API endpoints
- Default models
- Default parameters
- Timeout settings
- Retry configuration

Example:
```json
{
  "providers": {
    "qwen": {
      "base_url": "https://dashscope-intl.aliyuncs.com/compatible-mode/v1",
      "default_model": "qwen-flash",
      "default_params": {
        "temperature": 0.7,
        "max_tokens": 2000
      }
    }
  },
  "default_provider": "qwen",
  "timeout_seconds": 30,
  "retry_attempts": 3
}
```

## 🧪 Testing

### Run the Test Suite

```bash
cd test

# Option 1: Use the automated script
./run_api_tests.sh

# Option 2: Manual build and run
./build_test.sh
cd build_test
./test_api

# Run with API key (full integration test)
export QWEN_API_KEY="your-key"
./test_api
```

### Test Output

```
=== LLMEngine API Integration Tests ===
===============================================================
 Testing API Config Manager
===============================================================
✓ Config loaded successfully
Default provider: qwen
Timeout: 30 seconds
Retry attempts: 3

Available providers:
  - qwen
    Name: Qwen (DashScope)
    Base URL: https://dashscope-intl.aliyuncs.com/compatible-mode/v1
    Default Model: qwen-flash
...
```

## 🎯 Qwen Models

| Model | Description | Best For |
|-------|-------------|----------|
| `qwen-flash` | Fastest, most cost-effective | Quick responses, high volume, cost-sensitive |
| `qwen-turbo` | Fast, cost-effective | Quick responses, high volume |
| `qwen-plus` | Balanced performance | General tasks |
| `qwen-max` | Most capable | Complex reasoning |
| `qwen2.5-7b-instruct` | Smaller model | Low latency |
| `qwen2.5-72b-instruct` | Largest open model | Maximum capability |

## 🔒 Security Best Practices

1. **Never hardcode API keys**
   ```cpp
   // ❌ DON'T
   std::string api_key = "sk-actual-key";
   
   // ✅ DO
   const char* api_key = std::getenv("QWEN_API_KEY");
   ```

2. **Use environment variables**
   ```bash
   export QWEN_API_KEY="your-key"
   ```

3. **Or use a secure config file** (not in git)
   ```bash
   echo "config/secrets.json" >> .gitignore
   ```

## 🐛 Debugging

Enable debug mode to see detailed logs:

```cpp
LLMEngine engine(::LLMEngineAPI::ProviderType::QWEN, 
                 api_key, 
                 "qwen-flash",
                 {}, // params
                 24, // log retention hours
                 true); // debug mode

```

Debug files created:
- `api_response.json` - Full API response
- `api_response_error.json` - Error details
- `response_full.txt` - Complete response text
- `/tmp/llmengine/*.txt` - Analysis outputs

## 📊 Performance Tips

1. **Use qwen-flash for maximum speed**
   ```cpp
   LLMEngine engine(::LLMEngineAPI::ProviderType::QWEN, api_key, "qwen-flash");
   ```

2. **Limit tokens for faster responses**
   ```cpp
   auto result = engine.analyze(prompt, input, "test", 500); // max 500 tokens
   ```

3. **Adjust temperature for deterministic results**
   ```cpp
   nlohmann::json params = {{"temperature", 0.1}}; // more deterministic
   LLMEngine engine(::LLMEngineAPI::ProviderType::QWEN, api_key, "qwen-flash", params);
   ```

## ❓ Troubleshooting

### "API key not found"
```bash
# Check if environment variable is set
echo $QWEN_API_KEY

# Set it if missing
export QWEN_API_KEY="sk-your-key"
```

### "Network error"
- Check internet connection
- Verify firewall allows HTTPS to dashscope-intl.aliyuncs.com
- Try with debug mode enabled

### "Invalid model"
- Check model name spelling
- See available models in `config/api_config.json`
- Visit Qwen documentation for latest models

### "Rate limit exceeded"
- Wait a few seconds between requests
- Consider upgrading your Qwen API plan
- Implement exponential backoff in your code

## 📚 More Information

- **Full Documentation**: `config/README.md`
- **Test Examples**: `test/test_api.cpp`
- **Qwen Documentation**: https://help.aliyun.com/zh/dashscope/

## 🎓 Example Projects

### Simple Chatbot

```cpp
#include "LLMEngine.hpp"
#include <iostream>

int main() {
    LLMEngine engine(::LLMEngineAPI::ProviderType::QWEN, 
                     std::getenv("QWEN_API_KEY"), 
                     "qwen-flash");
    
    while (true) {
        std::string user_input;
        std::cout << "You: ";
        std::getline(std::cin, user_input);
        
        if (user_input == "quit") break;
        
        auto result = engine.analyze(user_input, {}, "chat");
        std::cout << "Bot: " << result[1] << std::endl << std::endl;
    }
    
    return 0;
}
```

### Code Analyzer

```cpp
LLMEngine engine(::LLMEngineAPI::ProviderType::QWEN, 
                 std::getenv("QWEN_API_KEY"), 
                 "qwen-max"); // Use most capable model

std::string code = "def factorial(n):\n    return n * factorial(n-1)";
std::string prompt = "Analyze this code and find bugs:";

nlohmann::json input = {
    {"code", code},
    {"language", "python"}
};

auto result = engine.analyze(prompt, input, "code_review");
std::cout << "Analysis: " << result[1] << std::endl;
```

## 🤝 Contributing

Found a bug or have a feature request? Please check the project's main README for contribution guidelines.

## 📄 License

This project is licensed under GPL v3. See LICENSE file for details.

---

**Ready to get started?** Set your QWEN_API_KEY and start coding! 🚀

