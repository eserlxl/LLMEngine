# LLMEngine

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/std/status)

LLMEngine is a modern C++17 library that provides a unified, type-safe interface to multiple Large Language Model (LLM) providers. It supports both local and online providers (Ollama, Qwen/DashScope, OpenAI, Anthropic) with a configuration-driven design, factory-based clients, and a comprehensive test suite.

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Quick Start](#quick-start)
- [Installation](#installation)
- [Usage](#usage)
- [Security](#security)
- [Performance](#performance)
- [Documentation](#documentation)
- [Contributing](#contributing)
- [License](#license)

## Overview

LLMEngine abstracts away provider-specific details and exposes a consistent API for text generation and analysis. It offers seamless switching between providers, sensible defaults via `config/api_config.json`, and preserves backward compatibility with legacy Ollama-only usage.

- **Project**: LLMEngine 0.1.0
- **Language/Std**: C++17
- **Build System**: CMake 3.16+
- **Platforms**: Linux, macOS, Windows

[↑ Back to top](#llmengine)

## Features

### Core
- **Multi-provider support**: Qwen (DashScope), OpenAI, Anthropic, and local Ollama
- **Unified interface**: One API across different providers and models
- **Config-driven**: Provider defaults and parameters from `config/api_config.json`
- **Backward compatible**: Existing Ollama code paths continue to work

### Developer Experience
- **Type-safe APIs** with enums and factory patterns
- **Examples**: Multiple example programs under `examples/`
- **Tests**: API integration tests under `test/`

### Providers
- Qwen (DashScope) — qwen-flash/qwen-plus/qwen-max/qwen2.5
- OpenAI — GPT-3.5, GPT-4 family
- Anthropic — Claude 3 series
- Ollama — any locally served model

[↑ Back to top](#llmengine)

## Quick Start

### Prerequisites
- C++17-compatible compiler
- CMake 3.16+
- Dependencies: OpenSSL, nlohmann_json, cpr

### Minimal Example (Qwen)

```cpp
#include "LLMEngine.hpp"
#include <iostream>

int main() {
    const char* api_key = std::getenv("QWEN_API_KEY");
    LLMEngine engine(::LLMEngineAPI::ProviderType::QWEN, api_key, "qwen-flash");
    auto result = engine.analyze("Explain quantum computing simply:", {}, "test");
    std::cout << "Response: " << result[1] << std::endl;
    return 0;
}
```

### Build Your Program

```bash
g++ -std=c++17 your_program.cpp -o your_program
pkg-config --cflags --libs libcpr nlohmann_json
```

[↑ Back to top](#llmengine)

## Installation

### From Source

```bash
git clone <repository-url>
cd LLMEngine
mkdir build
cd build
cmake ..
make -j20
sudo make install
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

## Usage

### Provider Selection

```cpp
// Qwen (recommended for speed/cost)
LLMEngine qwen(::LLMEngineAPI::ProviderType::QWEN, api_key, "qwen-flash");

// OpenAI
LLMEngine openai(::LLMEngineAPI::ProviderType::OPENAI, api_key, "gpt-3.5-turbo");

// Anthropic
LLMEngine claude(::LLMEngineAPI::ProviderType::ANTHROPIC, api_key, "claude-3-sonnet");

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

### API Keys

```bash
export QWEN_API_KEY="sk-your-qwen-key"
export OPENAI_API_KEY="sk-your-openai-key"
export ANTHROPIC_API_KEY="sk-your-anthropic-key"
```

[↑ Back to top](#llmengine)

## Security

- Do not hardcode API keys; use environment variables.
- Prefer secure endpoints (HTTPS) and verify firewall/network settings.
- Avoid committing secrets; keep sensitive files out of VCS.

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
auto result = engine.analyze(prompt, input, "test");
```

[↑ Back to top](#llmengine)

## Documentation

- Configuration guide: `config/README.md`
- Quick start: `QUICKSTART.md`
- API integration details: `API_INTEGRATION_SUMMARY.md`
- Examples: `examples/README.md`

[↑ Back to top](#llmengine)

## Contributing

We welcome contributions! Typical workflow:

```bash
git checkout -b feature/name
# make changes and add tests
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
