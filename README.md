# LLMEngine

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/std/status)

LLMEngine is a C++20 library that provides a unified, type-safe interface to multiple Large Language Model (LLM) providers. It supports local and online providers (Ollama, Qwen/DashScope, OpenAI, Anthropic) with a configuration-driven design, factory-based clients, and a comprehensive test suite.

Architecture overview:

```
┌─────────────┐     provider-agnostic API     ┌─────────────────────┐
│ Application │ ───────────────────────────▶ │     LLMEngine       │
└─────────────┘                               │  (factory, config)  │
                                              └─────────┬───────────┘
                                                        │
                                     selects provider   │
                                                        ▼
                   ┌───────────┬───────────┬───────────┬───────────┬───────────┐
                   │   Qwen    │  OpenAI   │ Anthropic │  Gemini   │   Ollama  │
                   └───────────┴───────────┴───────────┴───────────┴───────────┘
```

Key benefits:
- Consistent API across providers
- Config-driven defaults via `config/api_config.json`
- Simple model/provider switching
- Examples and tests included

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Quick Start](#quick-start)
- [Installation](#installation)
- [Build Presets](#build-presets)
- [Usage](#usage)
- [Security](#security)
- [Performance](#performance)
- [Documentation](#documentation)
- [Contributing](#contributing)
- [License](#license)

## Overview

LLMEngine abstracts away provider-specific details and exposes a consistent API for text generation and analysis. It offers seamless switching between providers, sensible defaults via `config/api_config.json`, and preserves backward compatibility with legacy Ollama-only usage.

- **Project**: LLMEngine 0.1.0
- **Language/Std**: C++20
- **Build System**: CMake 3.16+
- **Platforms**: Linux, macOS, Windows

[↑ Back to top](#llmengine)

## Features

### Core
- **Multi-provider support**: Qwen (DashScope), OpenAI, Anthropic, Google Gemini, and local Ollama
- **Unified interface**: One API across different providers and models
- **Config-driven**: Provider defaults and parameters from `config/api_config.json`
- **Backward compatible**: Existing Ollama code paths continue to work

### Developer Experience
- **Type-safe APIs** with enums and factory patterns
- **Examples**: Multiple example programs under `examples/`
- **Tests**: API integration tests under `test/`

### Providers
- Qwen (DashScope) — qwen-flash/qwen-plus/qwen2.5
- OpenAI — GPT-3.5, GPT-4 family, GPT-5
- Anthropic — Claude 3 series
- Ollama — any locally served model
- Google Gemini (AI Studio) — gemini-1.5-flash/pro

[↑ Back to top](#llmengine)

## Quick Start

### Prerequisites
- C++20-compatible compiler
- CMake 3.16+
- Dependencies: OpenSSL, nlohmann_json, cpr

### Minimal Example (Qwen)

```cpp
#include "LLMEngine.hpp"
#include <iostream>

int main() {
    const char* api_key = std::getenv("QWEN_API_KEY");
    LLMEngine engine(::LLMEngineAPI::ProviderType::QWEN, api_key, "qwen-flash");
    AnalysisResult result = engine.analyze("Explain quantum computing simply:", {}, "test");
    std::cout << "Response: " << result.content << std::endl;
    return 0;
}
```

### Build and Test the Library (from source)

```bash
cmake -S . -B build
cmake --build build --config Release -j20
ctest --test-dir build --output-on-failure
```

Then build and run an example:

```bash
bash examples/build_examples.sh
./examples/build_examples/chatbot
```

[↑ Back to top](#llmengine)

## Installation

### From Source

```bash
git clone <repository-url>
cd LLMEngine
cmake -S . -B build
cmake --build build --config Release -j20
```

### Dependencies

- OpenSSL
- nlohmann_json
- cpr

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

[↑ Back to top](#llmengine)

## Build Presets

This repo ships `CMakePresets.json` with ready-to-use configurations:

```bash
cmake --preset debug
cmake --preset relwithdebinfo
cmake --preset release
```

Granular toggles (examples):

```bash
cmake --preset debug -DLLM_ENABLE_ASAN=ON -DLLM_ENABLE_UBSAN=ON
cmake --preset release -DLLM_ENABLE_LTO=ON
cmake --preset debug -DENABLE_COVERAGE=ON
```

A consumer example using `find_package(LLMEngine)` exists in `examples/consumer`.

## Usage

### Provider Selection

```cpp
// Qwen (recommended for speed/cost)
LLMEngine qwen(::LLMEngineAPI::ProviderType::QWEN, api_key, "qwen-flash");

// OpenAI
LLMEngine openai(::LLMEngineAPI::ProviderType::OPENAI, api_key, "gpt-3.5-turbo");

// Anthropic
LLMEngine claude(::LLMEngineAPI::ProviderType::ANTHROPIC, api_key, "claude-3-sonnet");

// Google Gemini (AI Studio)
LLMEngine gemini(::LLMEngineAPI::ProviderType::GEMINI, api_key, "gemini-1.5-flash");

// Ollama (local)
LLMEngine ollama("http://localhost:11434", "llama2");
```

### Using Provider Names (string)

```cpp
LLMEngine engine("qwen", api_key, "qwen-flash");
LLMEngine engine2("qwen", api_key); // uses default model from config
```

### Custom Parameters

```cpp
nlohmann::json params = {{"temperature", 0.7}, {"max_tokens", 2000}, {"top_p", 0.9}};
LLMEngine engine(::LLMEngineAPI::ProviderType::QWEN, api_key, "qwen-plus", params);
```

### Configuration File

See `config/api_config.json` for provider defaults and parameters. Example excerpt:

```json
{
  "providers": {
    "qwen": {
      "base_url": "https://dashscope-intl.aliyuncs.com/compatible-mode/v1",
      "default_model": "qwen-flash",
      "default_params": {"temperature": 0.7, "max_tokens": 2000}
    },
    "openai": {
      "base_url": "https://api.openai.com/v1",
      "default_model": "gpt-3.5-turbo",
      "default_params": {"temperature": 0.7, "max_tokens": 2000}
    }
  },
  "default_provider": "qwen",
  "timeout_seconds": 30,
  "retry_attempts": 3
}
```

### Customizing Config File Path

The default configuration file path can be changed using the `APIConfigManager` API:

```cpp
#include "APIClient.hpp"

using namespace LLMEngineAPI;

// Get the config manager instance
auto& config_mgr = APIConfigManager::getInstance();

// Set a custom default path
config_mgr.setDefaultConfigPath("/custom/path/api_config.json");

// Query the current default path
std::string current_path = config_mgr.getDefaultConfigPath();

// Now loadConfig() without arguments will use the custom path
config_mgr.loadConfig();  // Uses "/custom/path/api_config.json"

// Or still override with explicit path when needed
config_mgr.loadConfig("/another/path/config.json");  // Uses explicit path
```

### API Keys

```bash
export QWEN_API_KEY="sk-your-qwen-key"
export OPENAI_API_KEY="sk-your-openai-key"
export ANTHROPIC_API_KEY="sk-your-anthropic-key"
export GEMINI_API_KEY="your-gemini-api-key"
```

### Running Analysis Requests

The `analyze()` method is the primary interface for making LLM requests:

```cpp
AnalysisResult result = engine.analyze("Explain quantum computing:", {}, "analysis");
if (result.success) {
    std::cout << "Response: " << result.content << std::endl;
    std::cout << "Thinking: " << result.think << std::endl;
} else {
    std::cerr << "Error: " << result.errorMessage << std::endl;
}
```

**Important Note on Prompt Modification**: By default, `analyze()` prepends a system instruction asking for brief, concise responses (one to two sentences). This behavior can be disabled by setting the `prepend_terse_instruction` parameter to `false`:

```cpp
// Use prompt verbatim without modification
AnalysisResult result = engine.analyze(
    "Your exact prompt here", 
    {}, 
    "analysis", 
    "chat", 
    false  // Disable terse-response instruction
);
```

This option is useful when you need precise control over prompts for evaluation or when integrating with downstream agents that require exact prompt matching.

[↑ Back to top](#llmengine)

## Security

LLMEngine has been designed with security best practices in mind. Follow these guidelines to ensure secure usage:

### API Key Management

- **Always use environment variables** for API keys. The library automatically checks environment variables first:
  - `QWEN_API_KEY`
  - `OPENAI_API_KEY`
  - `ANTHROPIC_API_KEY`
  - `GEMINI_API_KEY`

```bash
export QWEN_API_KEY="sk-your-qwen-key"
export OPENAI_API_KEY="sk-your-openai-key"
```

- **Never commit API keys** to version control. The `config/api_config.json` should only contain non-secret defaults.
- **Prefer environment variables** over config file values. The library prioritizes environment variables over config file entries.
- **Warning on config file fallback**: If an API key is not found in environment variables or constructor parameters, the library may fall back to `api_config.json`. When this happens, a warning is emitted to stderr. For production deployments, ensure API keys are always provided via environment variables to avoid this fallback.

### Debug Mode and Sensitive Data

When debug mode is enabled (`debug=true`), LLMEngine writes response artifacts to `/tmp/llmengine`. To protect sensitive data:

- **Debug files are automatically sanitized**: API keys and other sensitive fields are redacted before writing to disk.
- **Log retention is enforced**: Files older than `log_retention_hours` are automatically cleaned up.
- **Disable debug files in production**: Set the environment variable `LLMENGINE_DISABLE_DEBUG_FILES=1` to prevent any debug file writes.
- **Review debug files**: Before sharing debug files, verify that sensitive information has been properly redacted.
- **Secure directory permissions**: Debug artifact directories are automatically created with 0700 permissions (owner-only access) to prevent cross-user data exposure on shared hosts.

```cpp
// Debug mode with 24-hour retention
LLMEngine engine(provider_type, api_key, model, params, 24, true);

// Disable debug files entirely
setenv("LLMENGINE_DISABLE_DEBUG_FILES", "1", 1);
```

### Command Execution Security

The `Utils::execCommand()` function includes validation to prevent command injection:
- **Control characters are rejected**: Newlines, tabs, carriage returns, and all control characters are explicitly blocked
- **Shell metacharacters are rejected**: All shell metacharacters (|, &, ;, $, `, <, >, parentheses, brackets, wildcards) are blocked
- **Whitelist approach**: Only alphanumeric, single spaces, hyphens, underscores, dots, and slashes are allowed
- **Multiple spaces prevented**: Consecutive spaces are rejected to prevent obfuscation attempts

**Important Security Notes:**
- This function uses `popen()` which routes through a shell, so validation must be strict
- Only trusted commands should be passed to this function
- Never use `execCommand()` with untrusted user input
- For production use with untrusted input, consider using `posix_spawn` or `execve` with argv arrays instead

### Provider Selection

- **Unknown providers fail fast**: Invalid provider names throw exceptions instead of silently falling back to defaults.
- This prevents unexpected traffic routing in multi-tenant scenarios.

### Thread Safety

- **APIConfigManager is thread-safe**: Concurrent access is safe using reader-writer locks.
- Multiple threads can safely read configuration while one thread writes.

### Best Practices

- API keys must not be hardcoded; environment variables must be used.
- Secure endpoints (HTTPS) should be preferred and firewall/network settings should be verified.
- Secrets must not be committed; sensitive files must be kept out of version control.

Example:

```cpp
// Correct
const char* api_key = std::getenv("QWEN_API_KEY");
```

[↑ Back to top](#llmengine)

## Performance

- Use `qwen-flash` for fastest responses.
- Limit `max_tokens` to reduce latency and cost.
- Lower `temperature` for more deterministic output.

```cpp
nlohmann::json input = {{"max_tokens", 500}};
AnalysisResult result = engine.analyze(prompt, input, "test");
if (!result.success) {
    std::cerr << "Error: " << result.errorMessage << " (status " << result.statusCode << ")\n";
}
```

[↑ Back to top](#llmengine)

## Documentation

 - Quick start: [QUICKSTART.md](QUICKSTART.md)
 - Configuration: [docs/CONFIGURATION.md](docs/CONFIGURATION.md)
 - Providers: [docs/PROVIDERS.md](docs/PROVIDERS.md)
 - API reference: [docs/API_REFERENCE.md](docs/API_REFERENCE.md) (links to generated Doxygen)
 - FAQ: [docs/FAQ.md](docs/FAQ.md)
 - Examples: [examples/README.md](examples/README.md)

[↑ Back to top](#llmengine)

## Contributing

Contributions are welcome! Typical workflow:

```bash
git checkout -b feature/name
cd test
./run_api_tests.sh
git add -A
git commit -m "feat: add feature/name"
git push origin feature/name
```

[↑ Back to top](#llmengine)

## License

This project is licensed under the GNU General Public License v3.0 - see the [LICENSE](LICENSE) file for details.

[↑ Back to top](#llmengine)

## Migration Note (Breaking Change)

`LLMEngine::analyze` now returns `AnalysisResult` instead of `std::vector<std::string>`. Replace `result[1]` with `result.content` and `result[0]` with `result.think`. On failure, check `result.success` and read `result.errorMessage`/`result.statusCode`.
