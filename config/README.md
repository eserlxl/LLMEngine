# API Configuration Guide

This directory contains configuration files for the LLMEngine library's API integration with online AI models.

## Configuration File: `api_config.json`

The `api_config.json` file contains provider configurations for various AI services.

### Supported Providers

#### 1. Qwen (DashScope)
- **Base URL**: `https://dashscope-intl.aliyuncs.com/compatible-mode/v1`
- **Authentication**: API Key (Bearer token)
- **Models**: qwen-flash, qwen-turbo, qwen-plus, qwen-max, qwen2.5 series
- **Endpoint**: `/chat/completions`

#### 2. OpenAI
- **Base URL**: `https://api.openai.com/v1`
- **Authentication**: API Key (Bearer token)
- **Models**: gpt-4, gpt-4-turbo, gpt-3.5-turbo
- **Endpoint**: `/chat/completions`

#### 3. Anthropic Claude
- **Base URL**: `https://api.anthropic.com/v1`
- **Authentication**: API Key (x-api-key header)
- **Models**: claude-3 series, claude-2
- **Endpoint**: `/messages`

#### 4. Ollama (Local)
- **Base URL**: `http://localhost:11434` (configurable)
- **Authentication**: None
- **Models**: Any locally installed model
- **Endpoint**: `/api/generate`, `/api/chat`

## Usage Examples

### Using Qwen API

```cpp
#include "LLMEngine.hpp"

int main() {
    const char* api_key = std::getenv("QWEN_API_KEY");
    if (!api_key) { return 1; }
    std::string model = "qwen-flash";
    
    nlohmann::json params = {
        {"temperature", 0.7},
        {"max_tokens", 2000}
    };
    
    // Method 1: Using provider type
    LLMEngine engine(::LLMEngineAPI::ProviderType::QWEN, api_key, model, params);
    
    // Method 2: Using provider name (loads from config)
    LLMEngine engine2("qwen", api_key, model, params);
    
    // Make a request
    std::string prompt = "Explain quantum computing";
    nlohmann::json input = {{"context", "educational"}};
    AnalysisResult result = engine.analyze(prompt, input, "test");
    
    std::cout << result.content << std::endl; // Print response
    return 0;
}
```

### Using OpenAI API

```cpp
#include "LLMEngine.hpp"

int main() {
    const char* api_key = std::getenv("OPENAI_API_KEY");
    if (!api_key) { return 1; }
    
    LLMEngine engine(::LLMEngineAPI::ProviderType::OPENAI, api_key, "gpt-3.5-turbo");
    
    std::string prompt = "Write a haiku about programming";
    nlohmann::json input = {};
    AnalysisResult result = engine.analyze(prompt, input, "poetry");
    
    std::cout << result.content << std::endl;
    return 0;
}
```

### Backward Compatibility with Ollama

```cpp
#include "LLMEngine.hpp"

int main() {
    // Legacy constructor still works
    LLMEngine engine("http://localhost:11434", "llama2");
    
    std::string prompt = "Hello, how are you?";
    nlohmann::json input = {};
    AnalysisResult result = engine.analyze(prompt, input, "chat");
    
    std::cout << result.content << std::endl;
    return 0;
}
```

## Environment Variables

For security, it's recommended to store API keys in environment variables:

```bash
export QWEN_API_KEY="your-qwen-api-key"
export OPENAI_API_KEY="your-openai-api-key"
export ANTHROPIC_API_KEY="your-anthropic-api-key"
```

Then in your code:

```cpp
const char* api_key = std::getenv("QWEN_API_KEY");
if (api_key) {
    LLMEngine engine("qwen", api_key, "qwen-flash");
    // ...
}
```

## Configuration Parameters

### Top-level keys

- `default_provider` (string): Provider key used when no provider is specified explicitly.
- `timeout_seconds` (integer): Request timeout in seconds.
- `retry_attempts` (integer): Number of retries on transient failures (e.g., 429/5xx).
- `providers` (object): Map of provider key to configuration block.

### Provider block

- `base_url` (string): Base endpoint for the provider.
- `default_model` (string): Model used when none is explicitly provided.
- `default_params` (object): Default generation parameters merged with per-call params.

### Common Parameters

- **temperature** (0.0-2.0): Controls randomness. Lower = more focused, Higher = more creative
- **max_tokens**: Maximum tokens to generate
- **top_p** (0.0-1.0): Nucleus sampling threshold
- **frequency_penalty**: Penalize frequent tokens
- **presence_penalty**: Penalize tokens that have appeared

### Provider-Specific Parameters

**Qwen/OpenAI:**
- `frequency_penalty`, `presence_penalty`

**Anthropic:**
- No frequency/presence penalties

**Ollama:**
- `top_k`, `min_p`, `context_window`

## API Key Setup

### Qwen (DashScope)

1. Visit: https://dashscope.console.aliyun.com/
2. Create an account or log in
3. Navigate to API Keys section
4. Generate a new API key
5. Set the key: `export QWEN_API_KEY="your-key"`

### OpenAI

1. Visit: https://platform.openai.com/
2. Create an account or log in
3. Go to API Keys
4. Create new secret key
5. Set the key: `export OPENAI_API_KEY="your-key"`

### Anthropic

1. Visit: https://console.anthropic.com/
2. Create an account
3. Navigate to API Keys
4. Generate a new key
5. Set the key: `export ANTHROPIC_API_KEY="your-key"`

## Testing

Run the API integration tests:

```bash
cd test
mkdir build_api
cd build_api
cmake ..
make -j20
export QWEN_API_KEY="your-key"
./test_api
```

## Configuration File Location

The library looks for `api_config.json` in the following locations (in order):

1. Path specified in code: `config_mgr.loadConfig("/custom/path/api_config.json")`
2. Default path set via `config_mgr.setDefaultConfigPath()` (defaults to `config/api_config.json`)
3. `/usr/share/llmEngine/config/api_config.json` (after installation)

### Programmatically Setting Default Config Path

You can change the default configuration file path at runtime:

```cpp
#include "APIClient.hpp"

using namespace LLMEngineAPI;

auto& config_mgr = APIConfigManager::getInstance();

// Set a custom default path
config_mgr.setDefaultConfigPath("/custom/path/api_config.json");

// Query the current default path
std::string current_path = config_mgr.getDefaultConfigPath();

// Load using the default path (no arguments needed)
config_mgr.loadConfig();  // Uses "/custom/path/api_config.json"
```

This is useful when your application needs to load configuration from a non-standard location or when the config location is determined at runtime.

## Customizing Configuration

You can modify `api_config.json` to:

- Add custom base URLs for self-hosted APIs
- Change default models
- Adjust default parameters
- Add new providers (requires code changes)
- Configure timeout and retry settings

Example custom configuration:

```json
{
  "providers": {
    "custom_openai": {
      "name": "Custom OpenAI Compatible",
      "base_url": "https://my-custom-api.com/v1",
      "models": {
        "my-model": "custom-model-name"
      },
      "default_model": "my-model",
      "auth_type": "api_key",
      "headers": {
        "Content-Type": "application/json",
        "Authorization": "Bearer {api_key}"
      },
      "endpoints": {
        "chat": "/chat/completions"
      }
    }
  }
}
```

## Troubleshooting

### Common Issues

1. **Config file not found**: Ensure `api_config.json` is in the correct location
2. **API key errors**: Verify your API key is valid and has not expired
3. **Network errors**: Check your internet connection and firewall settings
4. **Rate limits**: Some providers have rate limits; implement appropriate delays
5. **Invalid model**: Ensure the model name exists for the provider

### Debug Mode

Enable debug mode to see detailed logs (avoid hardcoding keys; prefer environment variables):

```cpp
LLMEngine engine("qwen", api_key, "qwen-flash", params, 24, true); // debug=true
```

This will save responses to:
- `api_response.json` (successful responses)
- `api_response_error.json` (error responses)
- `response_full.txt` (full response text)

## License

This configuration and documentation is part of the LLMEngine library.
Licensed under the GNU General Public License version 3 (GPLv3).

## See also

- `docs/CONFIGURATION.md` — Full configuration guide and JSON examples
- `docs/PROVIDERS.md` — Provider feature matrix and mapping

