# Configuration

This document explains how to configure LLMEngine for online providers and local Ollama.

## File Locations

Search order for `api_config.json`:
1. Explicit path passed to the config manager in your code
2. `config/api_config.json` (project/local run)
3. `/usr/share/llmEngine/config/api_config.json` (after install)

Environment overrides: when both config and environment variables are present, the library uses API keys from environment variables and defaults (model/params) from the config file.

## Authentication

Use environment variables. Do not hardcode keys.

```bash
export QWEN_API_KEY="sk-..."
export OPENAI_API_KEY="sk-..."
export ANTHROPIC_API_KEY="sk-..."
```

In code:
```cpp
const char* apiKey = std::getenv("QWEN_API_KEY");
```

If an expected key is missing, ensure the environment variable is set before initializing the engine.

## Provider Endpoints (defaults)

- Qwen (DashScope): `https://dashscope-intl.aliyuncs.com/compatible-mode/v1`
- OpenAI: `https://api.openai.com/v1`
- Anthropic: `https://api.anthropic.com/v1`
- Ollama (local): `http://localhost:11434`

## Global Settings

These keys live at the root of `api_config.json`:

- `default_provider`: string provider key, e.g. `"qwen"`
- `timeout_seconds`: integer request timeout
- `retry_attempts`: integer retry count on transient failures

Example:
```json
{
  "default_provider": "qwen",
  "timeout_seconds": 30,
  "retry_attempts": 3
}
```

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

- Increase `timeout_seconds` for larger responses or slow networks.
- Set `retry_attempts` to handle transient HTTP 429/5xx.

## Logging and Debugging

Enable debug mode to write response artifacts for troubleshooting.

```cpp
LLMEngine engine(::LLMEngineAPI::ProviderType::QWEN, apiKey, "qwen-flash", {}, 24, true);
```

Artifacts:
- `api_response.json`
- `api_response_error.json`
- `response_full.txt`

## Security Notes

- Do not commit secrets.
- Use environment variables or secret managers.
- Prefer HTTPS for online providers.

## See Also

- `docs/PROVIDERS.md`
- `docs/API_REFERENCE.md`
- `config/README.md`
