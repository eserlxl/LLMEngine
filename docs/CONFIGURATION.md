# Configuration

This document explains how to configure LLMEngine for online providers and local Ollama.

## File Locations

Search order for `api_config.json`:
1. Explicit path passed to `loadConfig(path)` in application code
2. Default path set via `setDefaultConfigPath()` (defaults to `config/api_config.json`)
3. `/usr/share/llmEngine/config/api_config.json` (after installation)

### Changing the Default Config Path

The default configuration file path can be changed programmatically using the `APIConfigManager` API:

```cpp
#include "APIClient.hpp"

using namespace LLMEngineAPI;

auto& config_mgr = APIConfigManager::getInstance();

// Set a custom default path
config_mgr.setDefaultConfigPath("/custom/path/api_config.json");

// Query the current default path
std::string path = config_mgr.getDefaultConfigPath();  // Returns "/custom/path/api_config.json"

// Load using the default path
config_mgr.loadConfig();  // Uses "/custom/path/api_config.json"

// Or override with explicit path
config_mgr.loadConfig("/another/path/config.json");  // Uses explicit path
```

The default path is initialized to `config/api_config.json` when the library starts, but can be changed at runtime. All methods are thread-safe.

Environment overrides: when both config and environment variables are present, the library uses API keys from environment variables and defaults (model/params) from the config file.

## Authentication

Environment variables must be used for API keys. Keys must not be hardcoded in application code.

```bash
export QWEN_API_KEY="sk-..."
export OPENAI_API_KEY="sk-..."
export ANTHROPIC_API_KEY="sk-..."
```

In code:
```cpp
const char* apiKey = std::getenv("QWEN_API_KEY");
```

If an expected key is missing, the environment variable must be set before initializing the engine.

## Provider Endpoints (defaults)

- Qwen (DashScope): `https://dashscope-intl.aliyuncs.com/compatible-mode/v1`
- OpenAI: `https://api.openai.com/v1`
- Anthropic: `https://api.anthropic.com/v1`
- Ollama (local): `http://localhost:11434`

## Global Settings

These keys live at the root of `api_config.json`:

- `default_provider`: string provider key, e.g. `"qwen"`
- `timeout_seconds`: integer request timeout (default: 30 seconds)
- `retry_attempts`: integer retry count on transient failures

Example:
```json
{
  "default_provider": "qwen",
  "timeout_seconds": 30,
  "retry_attempts": 3
}
```

**Provider-Specific Timeouts**: Individual providers can override the global timeout by setting `timeout_seconds` in their provider configuration. For example, local Ollama uses a 300-second timeout by default to accommodate slower local processing:

```json
{
  "providers": {
    "ollama": {
      "timeout_seconds": 300,
      ...
    }
  }
}
```

When a provider-specific timeout is set, it takes precedence over the global timeout for that provider. Other providers without a specific timeout will use the global value.

Minimal `api_config.json` (schema skeleton):

```json
{
  "default_provider": "qwen",
  "timeout_seconds": 30,
  "retry_attempts": 3,
  "providers": {
    "qwen": {
      "base_url": "https://dashscope-intl.aliyuncs.com/compatible-mode/v1",
      "default_model": "qwen-flash",
      "default_params": {
        "temperature": 0.7,
        "max_tokens": 2000
      }
    }
  }
}
```

## Provider Defaults

Each provider can define `default_model` and `default_params`.

```json
{
  "providers": {
    "qwen": {
      "base_url": "https://dashscope-intl.aliyuncs.com/compatible-mode/v1",
      "default_model": "qwen-flash",
      "default_params": {"temperature": 0.7, "max_tokens": 2000}
    }
  }
}
```

Expanded example with multiple providers:

```json
{
  "default_provider": "qwen",
  "timeout_seconds": 30,
  "retry_attempts": 3,
  "providers": {
    "qwen": {
      "base_url": "https://dashscope-intl.aliyuncs.com/compatible-mode/v1",
      "default_model": "qwen-flash",
      "default_params": {
        "temperature": 0.7,
        "max_tokens": 2000,
        "top_p": 0.9
      }
    },
    "openai": {
      "base_url": "https://api.openai.com/v1",
      "default_model": "gpt-3.5-turbo",
      "default_params": {
        "temperature": 0.7,
        "max_tokens": 2000
      }
    },
    "anthropic": {
      "base_url": "https://api.anthropic.com/v1",
      "default_model": "claude-3-sonnet",
      "default_params": {
        "temperature": 0.7,
        "max_tokens": 2000
      }
    },
    "ollama": {
      "base_url": "http://localhost:11434",
      "default_model": "llama2",
      "default_params": {
        "temperature": 0.7,
        "max_tokens": 512
      }
    }
  }
}
```

## Timeouts and Retries

- `timeout_seconds` should be increased for larger responses or slow networks.
- `retry_attempts` should be set to handle transient HTTP 429/5xx errors.
- Backoff uses exponential backoff with full jitter. Global configuration keys:
  - `retry_attempts`, `retry_delay_ms` (base delay), optional `retry_max_delay_ms` (maximum delay cap)
- Per-request overrides are available via params: `retry_attempts`, `retry_base_delay_ms`, `retry_max_delay_ms`, and `jitter_seed` (for deterministic testing).

## Logging and Debugging

Debug mode can be enabled to write response artifacts for troubleshooting purposes.

```cpp
LLMEngine engine(::LLMEngineAPI::ProviderType::QWEN, apiKey, "qwen-flash", {}, 24, true);
```

Artifacts:
- `api_response.json`
- `api_response_error.json`
- `response_full.txt`

## Security Notes

- Secrets must not be committed to version control.
- Environment variables or secret managers must be used for API key storage.
- HTTPS should be preferred for online providers.

## See Also

- [docs/PROVIDERS.md](PROVIDERS.md) - Provider details and model information
- [docs/API_REFERENCE.md](API_REFERENCE.md) - API documentation
- [docs/SECURITY.md](SECURITY.md) - Security best practices
- [docs/PERFORMANCE.md](PERFORMANCE.md) - Performance optimization
- [QUICKSTART.md](../QUICKSTART.md) - Quick start guide
- [README.md](../README.md) - Main documentation
