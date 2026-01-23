# LLMEngine Quick Start

This guide shows the minimal steps to build the library, configure a provider, and run a request. For details, see [docs/configuration.md](docs/configuration.md), [docs/providers.md](docs/providers.md), and [docs/api_reference.md](docs/api_reference.md).

## 1. Minimal Example (Qwen)

```cpp
#include "LLMEngine.hpp"

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
    AnalysisResult result = engine.analyze(prompt, input, "test");
    
    // Print response
    std::cout << "Response: " << result.content << std::endl;
    
    return 0;
}
```

## 2. Obtaining a Qwen API Key

1. Visit: https://dashscope.console.aliyuncs.com/
2. Sign up or log in
3. Navigate to **API Keys** section
4. Click **Create API Key**
5. Copy the key and set it:
   ```bash
   export QWEN_API_KEY="sk-your-api-key-here"
   ```

## 3. Build and Run

```bash
cmake -S . -B build
cmake --build build --config Release -j20

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
        AnalysisResult result = engine.analyze(prompt, input, "math_test");
        
        std::cout << "Answer: " << result.content << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
EOF

# Compile
g++ -std=c++23 test_qwen.cpp -o test_qwen
pkg-config --cflags --libs libcpr nlohmann_json

# Run
export QWEN_API_KEY="your-key"
./test_qwen
```

## 4. Usage Examples

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

## 5. Configuration

### API Configuration File

Edit `config/api_config.json` to customize:
- API endpoints
- Default models
- Default parameters
- Timeout settings
- Retry configuration

### Using Provider Names (Config-based)

Instead of using provider enums, you can use provider names that are resolved via the configuration file:

```cpp
#include "LLMEngine/LLMEngine.hpp"

using namespace LLMEngine;

// Uses config file to resolve provider settings
LLMEngine engine("qwen", api_key, "qwen-flash");

// Or use default model from config
LLMEngine engine2("qwen", api_key);
```

This approach allows the configuration file to define provider endpoints, default models, and other settings, making it easier to switch between environments or providers without code changes.

### Custom Config File Path

The default config file path can be changed programmatically. This is useful when you need to:
- Use different config files for different environments (dev, staging, production)
- Store config files in custom locations
- Support multiple configuration files for different tenants

**Method 1: Set Default Config Path**

```cpp
#include "LLMEngine/APIClient.hpp"

using namespace LLMEngineAPI;

// Get the singleton config manager instance
auto& config_mgr = APIConfigManager::getInstance();

// Set a custom default path
config_mgr.setDefaultConfigPath("/custom/path/api_config.json");

// Query the current default path
std::string path = config_mgr.getDefaultConfigPath();

// Load using the default path
config_mgr.loadConfig();  // Uses "/custom/path/api_config.json"
```

**Method 2: Explicit Path in loadConfig()**

```cpp
auto& config_mgr = APIConfigManager::getInstance();

// Load from a specific path (overrides default)
bool success = config_mgr.loadConfig("/another/path/config.json");

if (!success) {
    // Handle configuration load failure
    std::cerr << "Failed to load configuration\n";
}
```

**Method 3: Dependency Injection via Constructor**

```cpp
#include "LLMEngine/LLMEngine.hpp"
#include "LLMEngine/APIClient.hpp"

using namespace LLMEngine;
using namespace LLMEngineAPI;

// Set up custom config path
auto& config_mgr = APIConfigManager::getInstance();
config_mgr.setDefaultConfigPath("/custom/path/api_config.json");
config_mgr.loadConfig();

// Wrap singleton in shared_ptr (with no-op deleter since it's a singleton)
auto config_manager = std::shared_ptr<IConfigManager>(
    &config_mgr, 
    [](IConfigManager*) {}  // No-op deleter
);

// Pass to LLMEngine constructor
LLMEngine::LLMEngine engine(
    "qwen",           // provider_name
    "",               // api_key (optional)
    "",               // model (optional, uses config default)
    {},               // model_params
    24,               // log_retention_hours
    false,            // debug
    config_manager    // custom config manager
);
```

For comprehensive documentation on setting custom config paths, including thread safety, API reference, and best practices, see [docs/custom_config_path.md](docs/custom_config_path.md).

See [docs/configuration.md](docs/configuration.md) for complete configuration details.

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

## 6. Testing

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

## 7. Qwen Models

| Model | Description | Best For |
|-------|-------------|----------|
| `qwen-flash` | Fastest, most cost-effective | Quick responses, high volume, cost-sensitive |
| `qwen-turbo` | Fast, cost-effective | Quick responses, high volume |
| `qwen-plus` | Balanced performance | General tasks |
| `qwen-max` | Most capable | Complex reasoning |
| `qwen2.5-7b-instruct` | Smaller model | Low latency |
| `qwen2.5-72b-instruct` | Largest open model | Maximum capability |

## 8. Security Best Practices

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

## 9. Debugging

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

## 10. Performance Tips

1. **Use qwen-flash for maximum speed**
   ```cpp
   LLMEngine engine(::LLMEngineAPI::ProviderType::QWEN, api_key, "qwen-flash");
   ```

2. **Limit tokens for faster responses**
   ```cpp
   AnalysisResult result = engine.analyze(prompt, input, "test");
   ```

3. **Adjust temperature for deterministic results**
   ```cpp
   nlohmann::json params = {{"temperature", 0.1}}; // more deterministic
   LLMEngine engine(::LLMEngineAPI::ProviderType::QWEN, api_key, "qwen-flash", params);
   ```

## 11. Troubleshooting

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
- Consider upgrading the Qwen API plan
- Implement exponential backoff in application code

## 12. More Information

- **Full Documentation**: See [README.md](README.md) for overview and [docs/api_reference.md](docs/api_reference.md) for API details
- **Configuration**: See [docs/configuration.md](docs/configuration.md)
- **Custom Config Paths**: See [docs/custom_config_path.md](docs/custom_config_path.md)
- **Providers**: See [docs/providers.md](docs/providers.md)
- **Security**: See [docs/security.md](docs/security.md)
- **Performance**: See [docs/performance.md](docs/performance.md)
- **Examples**: See [examples/README.md](examples/README.md)
- **Test Examples**: `test/test_api.cpp`
- **Qwen Documentation**: https://help.aliyun.com/zh/dashscope/

## 13. Example Projects

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
        
        AnalysisResult result = engine.analyze(user_input, {}, "chat");
        std::cout << "Bot: " << result.content << std::endl << std::endl;
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

AnalysisResult result = engine.analyze(prompt, input, "code_review");
std::cout << "Analysis: " << result.content << std::endl;
```

## 14. Contributing

Found a bug or have a feature request? Please check the [main README](README.md) and [.github/contributing.md](.github/contributing.md) for contribution guidelines.

## 15. License

This project is licensed under GPL v3. See LICENSE file for details.

---

To get started, set the `QWEN_API_KEY` environment variable and begin coding.

