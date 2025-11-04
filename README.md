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

## Contents

- [Overview](#overview)
- [Features](#features)
- [Quick Start](#quick-start)
- [Installation](#installation)
- [Build Presets](#build-presets)
- [Usage](#usage)
- [Documentation](#documentation)
- [Contributing](#contributing)
- [License](#license)

## Overview

LLMEngine abstracts away provider-specific details and exposes a consistent API for text generation and analysis. It offers seamless switching between providers with sensible defaults via `config/api_config.json`.

- **Project**: LLMEngine 0.1.0
- **Language/Std**: C++20
- **Build System**: CMake 3.20+
- **Platforms**: Linux, macOS

[↑ Back to top](#llmengine)

## Features

### Core
- **Multi-provider support**: Qwen (DashScope), OpenAI, Anthropic, Google Gemini, and local Ollama
- **Unified interface**: One API across different providers and models
- **Config-driven**: Provider defaults and parameters from `config/api_config.json`

### Developer Experience
- **Type-safe APIs** with enums and factory patterns
- **Examples**: Multiple example programs under `examples/`
- **Tests**: API integration tests under `test/`

### Providers
- Qwen (DashScope) — qwen-flash/qwen-plus/qwen2.5
- OpenAI — GPT-3.5, GPT-4 family
- Anthropic — Claude 3 series
- Ollama — any locally served model
- Google Gemini (AI Studio) — gemini-1.5-flash/pro

[↑ Back to top](#llmengine)

## Quick Start

### Prerequisites
- C++20-compatible compiler
- CMake 3.20+
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

Build with make:

```bash
cmake --build build -j20
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
LLMEngine ollama(::LLMEngineAPI::ProviderType::OLLAMA, "", "llama2");
// Or using provider name:
LLMEngine ollama2("ollama", "", "llama2");
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

## Documentation

 - Quick start: [QUICKSTART.md](QUICKSTART.md)
 - Configuration: [docs/CONFIGURATION.md](docs/CONFIGURATION.md)
 - Providers: [docs/PROVIDERS.md](docs/PROVIDERS.md)
 - API reference: [docs/API_REFERENCE.md](docs/API_REFERENCE.md) (links to generated Doxygen)
 - FAQ: [docs/FAQ.md](docs/FAQ.md)
 - Security: [docs/SECURITY.md](docs/SECURITY.md)
 - Performance: [docs/PERFORMANCE.md](docs/PERFORMANCE.md)
 - Contributing: [.github/CONTRIBUTING.md](.github/CONTRIBUTING.md)
 - Examples: [examples/README.md](examples/README.md)

[↑ Back to top](#llmengine)

## License

This project is licensed under the GNU General Public License v3.0 - see the [LICENSE](LICENSE) file for details.

[↑ Back to top](#llmengine)