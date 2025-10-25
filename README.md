# LLMEngine

A powerful C++ library for integrating Large Language Models (LLMs) with support for both local and online AI providers. LLMEngine provides a unified interface for interacting with various AI models including Ollama (local), Qwen (DashScope), OpenAI, and Anthropic Claude.

## 🚀 Features

- **Multi-Provider Support**: Seamlessly switch between local and online AI providers
- **Backward Compatibility**: Existing Ollama-only code continues to work without changes
- **Configuration-Driven**: Easy provider management through JSON configuration
- **Type Safety**: Strong typing with enums and factory patterns
- **Comprehensive Testing**: Full test suite included
- **Cross-Platform**: Works on Linux, macOS, and Windows
- **Modern C++**: Built with C++17 standards

## 📋 Supported Providers

| Provider | Type | Models | Status |
|----------|------|--------|--------|
| **Ollama** | Local | Any Ollama model | ✅ Stable |
| **Qwen (DashScope)** | Online | qwen-flash, qwen-plus, qwen-max, qwen2.5 series | ✅ Stable |
| **OpenAI** | Online | GPT-3.5, GPT-4, etc. | ✅ Stable |
| **Anthropic** | Online | Claude-3 Sonnet, Haiku, Opus | ✅ Stable |

## 🛠️ Dependencies

- **C++17** or later
- **CMake** 3.14+
- **nlohmann/json** - JSON handling
- **cpr** - HTTP client library
- **OpenSSL** - SSL/TLS support

### Installing Dependencies

#### Ubuntu/Debian
```bash
sudo apt update
sudo apt install build-essential cmake libssl-dev
sudo apt install nlohmann-json3-dev libcpr-dev
```

#### Arch Linux
```bash
sudo pacman -S base-devel cmake openssl
sudo pacman -S nlohmann-json cpr
```

#### macOS (Homebrew)
```bash
brew install cmake openssl nlohmann-json cpr
```

## 🔧 Installation

### Building from Source

```bash
# Clone the repository
git clone <repository-url>
cd LLMEngine

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build the library
make -j20

# Install (optional)
sudo make install
```

### Quick Build Script

```bash
# Use the provided build script
cd test
./build_test.sh
```

## 🎯 Quick Start

### 1. Using Qwen API (Recommended for Testing)

```cpp
#include "LLMEngine.hpp"
#include <iostream>

int main() {
    // Get API key from environment
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

### 2. Using Ollama (Local)

```cpp
#include "LLMEngine.hpp"

int main() {
    // Create engine with local Ollama
    LLMEngine engine("http://localhost:11434", "llama2");
    
    std::string prompt = "Hello, how are you?";
    nlohmann::json input = {};
    auto result = engine.analyze(prompt, input, "chat");
    
    std::cout << result[1] << std::endl;
    return 0;
}
```

### 3. Compiling Your Program

```bash
g++ -std=c++17 your_program.cpp -o your_program \
    -I./src \
    -L./build -lLLMEngine \
    $(pkg-config --cflags --libs libcpr nlohmann_json)
```

## 🔑 API Key Setup

### Qwen (DashScope) API Key

1. Visit: https://dashscope.console.aliyuncs.com/
2. Sign up or log in
3. Navigate to **API Keys** section
4. Click **Create API Key**
5. Set environment variable:
   ```bash
   export QWEN_API_KEY="sk-your-api-key-here"
   ```

### OpenAI API Key

```bash
export OPENAI_API_KEY="sk-your-openai-key"
```

### Anthropic API Key

```bash
export ANTHROPIC_API_KEY="sk-ant-your-anthropic-key"
```

## 📖 Usage Examples

### Different Provider Types

```cpp
// Qwen (fastest, most cost-effective)
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

### Custom Parameters

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

### Provider Information

```cpp
LLMEngine engine(::LLMEngineAPI::ProviderType::QWEN, api_key, "qwen-flash");

std::cout << "Provider: " << engine.getProviderName() << std::endl;
std::cout << "Is online: " << (engine.isOnlineProvider() ? "Yes" : "No") << std::endl;
```

## 🎯 Qwen Models Comparison

| Model | Description | Best For |
|-------|-------------|----------|
| `qwen-flash` | Fastest, most cost-effective | Quick responses, high volume, cost-sensitive |
| `qwen-turbo` | Fast, cost-effective | Quick responses, high volume |
| `qwen-plus` | Balanced performance | General tasks |
| `qwen-max` | Most capable | Complex reasoning |
| `qwen2.5-7b-instruct` | Smaller model | Low latency |
| `qwen2.5-72b-instruct` | Largest open model | Maximum capability |

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

### Test Output Example

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

## ⚙️ Configuration

### API Configuration File (`config/api_config.json`)

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
    },
    "openai": {
      "base_url": "https://api.openai.com/v1",
      "default_model": "gpt-3.5-turbo",
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

## 🏗️ Architecture

```
LLMEngine (Main Class)
├── use_api_client_ (bool)
├── api_client_ (unique_ptr<APIClient>)
│   ├── QwenClient
│   ├── OpenAIClient  
│   ├── AnthropicClient
│   └── OllamaClient
└── Legacy Ollama support (direct HTTP)

APIClientFactory
├── createClient()
└── createClientFromConfig()

APIConfigManager (Singleton)
├── loadConfig()
├── getProviderConfig()
└── getAvailableProviders()
```

## 📊 Performance Tips

1. **Use qwen-flash for maximum speed**
   ```cpp
   LLMEngine engine(::LLMEngineAPI::ProviderType::QWEN, api_key, "qwen-flash");
   ```

2. **Limit tokens for faster responses**
   ```cpp
   nlohmann::json input = {{"max_tokens", 500}}; // max 500 tokens
   auto result = engine.analyze(prompt, input, "test");
   ```

3. **Adjust temperature for deterministic results**
   ```cpp
   nlohmann::json params = {{"temperature", 0.1}}; // more deterministic
   LLMEngine engine(::LLMEngineAPI::ProviderType::QWEN, api_key, "qwen-flash", params);
   ```

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

3. **Secure config files** (not in git)
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

### Build Errors
If you encounter build errors:
1. All files use the correct namespace (`LLMEngineAPI`)
2. Header files are properly included
3. CMake cache is cleared if needed: `rm -rf build && mkdir build && cd build && cmake ..`

## 📚 Example Projects

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

## 📁 Project Structure

```
LLMEngine/
├── src/                    # Source code
│   ├── LLMEngine.hpp      # Main class header
│   ├── LLMEngine.cpp      # Main class implementation
│   ├── APIClient.hpp      # API client interface
│   ├── APIClient.cpp      # API client implementations
│   ├── LLMOutputProcessor.hpp
│   ├── LLMOutputProcessor.cpp
│   ├── Utils.hpp
│   └── Utils.cpp
├── config/                 # Configuration files
│   ├── api_config.json    # API provider configuration
│   └── README.md          # Configuration documentation
├── test/                   # Test suite
│   ├── test_api.cpp       # API integration tests
│   ├── build_test.sh      # Build script
│   └── run_api_tests.sh   # Test runner
├── CMakeLists.txt         # Build configuration
├── LICENSE                # GPL v3 license
├── QUICKSTART.md          # Quick start guide
└── README.md              # This file
```

## 🤝 Contributing

We welcome contributions! Please follow these guidelines:

1. **Fork the repository**
2. **Create a feature branch**: `git checkout -b feature-name`
3. **Make your changes** and add tests
4. **Run the test suite**: `cd test && ./run_api_tests.sh`
5. **Commit your changes**: `git commit -m "Add feature-name"`
6. **Push to the branch**: `git push origin feature-name`
7. **Submit a pull request**

### Development Setup

```bash
# Clone your fork
git clone https://github.com/yourusername/LLMEngine.git
cd LLMEngine

# Create build directory
mkdir build && cd build
cmake ..
make -j20

# Run tests
cd ../test
./run_api_tests.sh
```

## 📄 License

This project is licensed under the **GNU General Public License v3.0** - see the [LICENSE](LICENSE) file for details.

## 📞 Support

- **Documentation**: Check `config/README.md` for detailed configuration guide
- **Quick Start**: See `QUICKSTART.md` for immediate setup
- **API Integration**: Review `API_INTEGRATION_SUMMARY.md` for technical details
- **Issues**: Report bugs and request features via GitHub Issues

## 🎓 Learning Resources

- **Qwen Documentation**: https://help.aliyun.com/zh/dashscope/
- **OpenAI API Docs**: https://platform.openai.com/docs
- **Anthropic Claude Docs**: https://docs.anthropic.com/
- **Ollama Documentation**: https://ollama.ai/docs

---

**Ready to get started?** Set your `QWEN_API_KEY` and start coding! 🚀
