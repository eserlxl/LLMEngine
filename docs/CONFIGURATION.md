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

### Required Environment Variables

**API keys default to environment variables** for security. The library will automatically check for these environment variables before using any keys provided via constructor parameters or configuration files.

**Required environment variables by provider:**

| Provider | Environment Variable | Required |
|----------|---------------------|----------|
| Qwen (DashScope) | `QWEN_API_KEY` | Yes (for Qwen) |
| OpenAI | `OPENAI_API_KEY` | Yes (for OpenAI) |
| Anthropic | `ANTHROPIC_API_KEY` | Yes (for Anthropic) |
| Gemini | `GEMINI_API_KEY` | Yes (for Gemini) |
| Ollama | None | No (local only) |

**Example setup:**
```bash
export QWEN_API_KEY="sk-..."
export OPENAI_API_KEY="sk-..."
export ANTHROPIC_API_KEY="sk-..."
export GEMINI_API_KEY="..."
```

**Security Best Practices:**
- API keys must **never** be hardcoded in application code
- Keys must **never** be committed to version control
- Use environment variables or secret managers (e.g., Kubernetes secrets, AWS Secrets Manager) for production deployments
- The library will log a warning if API keys are read from configuration files instead of environment variables

**Priority order for API keys:**
1. Environment variables (highest priority, most secure)
2. Constructor parameters (for testing/development only)
3. Configuration file (not recommended, will log warnings)

If an expected key is missing for a provider that requires authentication, the library will fail fast with a clear error message.

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

### Logging Behavior

The library uses a pluggable `Logger` interface for all diagnostic output. By default, a `DefaultLogger` is used that writes to `std::cerr`. Applications can provide a custom logger implementation to integrate with their logging infrastructure.

**Setting a custom logger:**
```cpp
#include "LLMEngine/LLMEngine.hpp"
#include "LLMEngine/Logger.hpp"

using namespace LLMEngine;

class MyLogger : public Logger {
public:
    void log(LogLevel level, const std::string& message) override {
        // Integrate with your logging system
        my_logging_system.log(level, message);
    }
};

// Set custom logger
auto logger = std::make_shared<MyLogger>();
LLMEngine::LLMEngine engine(...);
engine.setLogger(logger);
```

**Log levels:**
- `LLMEngine::LogLevel::Debug`: Detailed diagnostic information (debug artifacts, request/response details)
- `LLMEngine::LogLevel::Info`: General informational messages
- `LLMEngine::LogLevel::Warn`: Warning messages (e.g., failed artifact writes, API key from config file)
- `LLMEngine::LogLevel::Error`: Error messages (e.g., API failures, configuration errors)

**Environment variable for request logging:**
Set `LLMENGINE_LOG_REQUESTS=1` to enable detailed HTTP request logging (useful for debugging network issues).

### Debug Artifacts

Debug mode can be enabled to write response artifacts for troubleshooting purposes. Artifacts are written to a unique temporary directory per request to avoid conflicts in concurrent scenarios.

**Enabling debug mode:**
```cpp
LLMEngine engine(::LLMEngineAPI::ProviderType::QWEN, apiKey, "qwen-flash", {}, 24, true);
//                                                                                        ^^^^
//                                                                                    debug = true
```

**Artifact locations:**
- Base directory: `/tmp/llmengine` (configurable via `Utils::TMP_DIR`)
- Per-request directory: `/tmp/llmengine/req_<timestamp>_<thread_hash>`
- Artifact files:
  - `api_response.json`: Full API response (secrets redacted)
  - `api_response_error.json`: Error response (secrets redacted)
  - `response_full.txt`: Full response text (secrets redacted)
  - `<analysis_type>.think.txt`: Extracted reasoning section
  - `<analysis_type>.txt`: Remaining content after reasoning extraction

**Artifact retention:**
- Artifacts are automatically cleaned up after `log_retention_hours` (default: 24 hours)
- Set `LLMENGINE_DISABLE_DEBUG_FILES=1` to disable debug file writing even when debug mode is enabled
- Applications can inject a dynamic policy via `engine.setDebugFilesPolicy([]{ return /* enabled? */; });` to control artifact writing at runtime per request or per tenant.

**Error handling:**
- Failed artifact writes are logged as warnings but do not fail the request
- Operators can check logs to identify when artifacts are missing (e.g., due to permissions)

## Ollama TLS Configuration

Ollama does not natively support TLS/HTTPS. To use Ollama with TLS verification enabled, you need to set up a reverse proxy (such as NGINX or Caddy) that handles TLS termination and forwards requests to your local Ollama instance.

### Setting Up Ollama with TLS

**Option 1: Using NGINX as a Reverse Proxy**

1. **Install NGINX** (if not already installed):
   ```bash
   # On Ubuntu/Debian
   sudo apt install nginx
   
   # On Arch Linux
   sudo pacman -S nginx
   ```

2. **Generate or obtain TLS certificates**:
   - For production: Use Let's Encrypt with Certbot
   - For local development: Generate self-signed certificates:
     ```bash
     sudo openssl req -x509 -nodes -days 365 -newkey rsa:2048 \
       -keyout /etc/ssl/private/ollama.key \
       -out /etc/ssl/certs/ollama.crt
     ```

3. **Configure NGINX** (`/etc/nginx/sites-available/ollama`):
   ```nginx
   server {
       listen 11435 ssl;
       server_name localhost;

       ssl_certificate /etc/ssl/certs/ollama.crt;
       ssl_certificate_key /etc/ssl/private/ollama.key;

       location / {
           proxy_pass http://localhost:11434;
           proxy_set_header Host $host;
           proxy_set_header X-Real-IP $remote_addr;
           proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
           proxy_set_header X-Forwarded-Proto $scheme;
       }
   }
   ```

4. **Enable and start NGINX**:
   ```bash
   sudo ln -s /etc/nginx/sites-available/ollama /etc/nginx/sites-enabled/
   sudo nginx -t  # Test configuration
   sudo systemctl restart nginx
   ```

**Option 2: Using Caddy (Automatic HTTPS)**

Caddy automatically handles TLS certificates via Let's Encrypt:

1. **Install Caddy** and create `Caddyfile`:
   ```
   localhost:11435 {
       reverse_proxy localhost:11434
   }
   ```

2. **Start Caddy**:
   ```bash
   caddy run
   ```

### Configuring LLMEngine to Use TLS

Once your Ollama instance is accessible via HTTPS, configure LLMEngine to use it:

**Method 1: Update base URL and enable TLS verification in code**

The base URL is read from the configuration file. Update `config/api_config.json` to use HTTPS, then enable TLS verification in your code:

```cpp
#include "LLMEngine.hpp"
#include <nlohmann/json.hpp>

int main() {
    std::string model = "llama2";
    
    // Enable TLS verification in model parameters
    nlohmann::json params = {
        {"verify_ssl", true},  // Enable TLS verification
        {"temperature", 0.7}
    };
    
    // Create engine with Ollama provider
    // The base_url will be read from config/api_config.json
    LLMEngine engine(::LLMEngineAPI::ProviderType::OLLAMA, 
                     "",  // No API key needed for Ollama
                     model,
                     params);
    
    // Use the engine...
}
```

**Method 2: Update configuration file**

Update `config/api_config.json`:

```json
{
  "providers": {
    "ollama": {
      "base_url": "https://localhost:11435",
      "default_model": "llama2",
      "default_params": {
        "verify_ssl": true,
        "temperature": 0.7
      }
    }
  }
}
```

**Method 3: Pass verify_ssl in model_params during construction**

```cpp
nlohmann::json model_params = {
    {"verify_ssl", true},  // Enable TLS verification
    {"temperature", 0.7},
    {"max_tokens", 2000}
};

LLMEngine engine(::LLMEngineAPI::ProviderType::OLLAMA,
                 "",
                 "llama2",
                 model_params);
```

### Important Notes

- **Self-signed certificates**: If using self-signed certificates for local development, you may need to add the certificate to your system's trust store, or the verification will fail even with `verify_ssl: true`.
- **Certificate validation**: When `verify_ssl: true` is set, the client will verify the certificate chain. For self-signed certificates, you may need to:
  - Add the certificate to the system CA bundle, or
  - Use a tool like `mkcert` to create locally-trusted certificates
- **Default behavior**: By default, Ollama uses `verify_ssl: false` to accommodate local instances that often use self-signed certificates. The library will log a warning (once per process) when TLS verification is disabled.
- **Production**: Always use proper TLS certificates from a trusted CA in production environments.

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
