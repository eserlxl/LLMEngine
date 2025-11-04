# LLMEngine

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/std/status)

**LLMEngine** is a modern **C++20 library** that provides a unified, type-safe interface for interacting with multiple **Large Language Model (LLM)** providers.  
It supports both local and cloud-based backends — **Ollama**, **Qwen (DashScope)**, **OpenAI**, **Anthropic**, and **Gemini (AI Studio)** — with a flexible, configuration-driven architecture.

---

## 🚀 Architecture Overview

```text
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

---

## 🌟 Key Highlights

- **Consistent API** across all supported providers.  
- **Configuration-first design** via `config/api_config.json`.  
- **Factory-based provider selection** for seamless switching.  
- **Comprehensive examples** and **test suite** included.

---

## 📖 Contents

- [Overview](#overview)
- [Features](#features)
- [Quick Start](#quick-start)
- [Installation](#installation)
- [Build Presets](#build-presets)
- [Usage](#usage)
- [Configuration](#configuration)
- [API Keys](#api-keys)
- [Running Analysis Requests](#running-analysis-requests)
- [Documentation](#documentation)
- [License](#license)

<a id="overview"></a>
## 🧩 Overview

LLMEngine abstracts away the differences between major LLM providers, offering a **unified C++ API** for text generation, reasoning, and analysis.  
Its configuration-driven design allows rapid experimentation and reliable runtime switching between providers.

| Property | Value |
|-----------|--------|
| **Project** | LLMEngine 0.1.0 |
| **Language** | C++20 |
| **Build System** | CMake 3.20+ |
| **Supported Platforms** | Linux, macOS |

[↑ Back to top](#llmengine)

---

<a id="features"></a>
## ⚙️ Features

### Core

- **Multi-provider support:** Qwen, OpenAI, Anthropic, Gemini, Ollama.  
- **Unified interface:** Same API, different models.  
- **JSON configuration:** All defaults live in `config/api_config.json`.

### Developer Experience

- Type-safe, modern C++ API (with enums, smart pointers, and factories).  
- Built-in examples under `examples/`.  
- Integration and regression tests under `test/`.

### Supported Providers

| Provider | Example Models |
|-----------|----------------|
| **Qwen (DashScope)** | qwen-flash, qwen-plus, qwen2.5 |
| **OpenAI** | GPT-3.5, GPT-4 series |
| **Anthropic** | Claude 3 series |
| **Gemini (AI Studio)** | gemini-1.5-flash, gemini-1.5-pro |
| **Ollama (Local)** | Any locally hosted model |

[↑ Back to top](#llmengine)

---

<a id="quick-start"></a>
## ⚡ Quick Start

### Prerequisites

- C++20 compiler (e.g., GCC 11+, Clang 14+, MSVC 2022)  
- CMake 3.20+  
- Dependencies: **OpenSSL**, **nlohmann_json**, **cpr**

### Minimal Example

```cpp
#include "LLMEngine.hpp"
#include <iostream>

int main() {
    const char* api_key = std::getenv("QWEN_API_KEY");
    LLMEngine engine(::LLMEngineAPI::ProviderType::QWEN, api_key, "qwen-flash");
    auto result = engine.analyze("Explain quantum computing simply:", {}, "demo");
    std::cout << result.content << std::endl;
    return 0;
}
```

### Build and Test

```bash
cmake -S . -B build
cmake --build build --config Release -j$(nproc)
ctest --test-dir build --output-on-failure
```

Example execution:

```bash
bash examples/build_examples.sh
./examples/build_examples/chatbot
```

[↑ Back to top](#llmengine)

---

<a id="installation"></a>
## 🧱 Installation

### From Source

```bash
git clone <repository-url>
cd LLMEngine
cmake -S . -B build
cmake --build build --config Release -j$(nproc)
```

### Install Dependencies

**Ubuntu/Debian**
```bash
sudo apt install build-essential cmake libssl-dev nlohmann-json3-dev libcpr-dev
```

**Arch Linux**
```bash
sudo pacman -S base-devel cmake openssl nlohmann-json cpr
```

**macOS (Homebrew)**
```bash
brew install cmake openssl nlohmann-json cpr
```

[↑ Back to top](#llmengine)

---

<a id="build-presets"></a>
## 🧰 Build Presets

The repo includes a `CMakePresets.json` file with ready-to-use presets:

```bash
cmake --preset debug
cmake --preset relwithdebinfo
cmake --preset release
```

Build with make:

```bash
cmake --build build -j$(nproc)
```

Optional build toggles:

```bash
cmake --preset debug -DLLM_ENABLE_ASAN=ON -DLLM_ENABLE_UBSAN=ON
cmake --preset release -DLLM_ENABLE_LTO=ON
cmake --preset debug -DENABLE_COVERAGE=ON
```

A consumer example using `find_package(LLMEngine)` exists in `examples/consumer`.

[↑ Back to top](#llmengine)

---

<a id="usage"></a>
## 💻 Usage

### Provider Initialization

```cpp
LLMEngine qwen(::LLMEngineAPI::ProviderType::QWEN, api_key, "qwen-flash");
LLMEngine openai(::LLMEngineAPI::ProviderType::OPENAI, api_key, "gpt-4-turbo");
LLMEngine claude(::LLMEngineAPI::ProviderType::ANTHROPIC, api_key, "claude-3-sonnet");
LLMEngine gemini(::LLMEngineAPI::ProviderType::GEMINI, api_key, "gemini-1.5-flash");
LLMEngine ollama(::LLMEngineAPI::ProviderType::OLLAMA, "", "llama3");
// Or using provider name:
LLMEngine ollama2("ollama", "", "llama2");
```

### Using Provider Names

```cpp
LLMEngine engine("qwen", api_key, "qwen-flash");
LLMEngine engine2("qwen", api_key);  // Uses default model from config
```

### Custom Parameters

```cpp
nlohmann::json params = {
    {"temperature", 0.7},
    {"max_tokens", 2000},
    {"top_p", 0.9}
};
LLMEngine engine(::LLMEngineAPI::ProviderType::QWEN, api_key, "qwen-plus", params);
```

---

<a id="configuration"></a>
## 🧩 Configuration

The default config file defines provider endpoints and defaults:

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

To change configuration paths programmatically:

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

[↑ Back to top](#llmengine)

---

<a id="api-keys"></a>
## 🔑 API Keys

```bash
export QWEN_API_KEY="sk-your-qwen-key"
export OPENAI_API_KEY="sk-your-openai-key"
export ANTHROPIC_API_KEY="sk-your-anthropic-key"
export GEMINI_API_KEY="your-gemini-api-key"
```

[↑ Back to top](#llmengine)

---

<a id="running-analysis-requests"></a>
## 🧠 Running Analysis Requests

The `analyze()` method is the primary interface for making LLM requests:

```cpp
auto result = engine.analyze("Explain quantum computing:", {}, "analysis");
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
auto result = engine.analyze(
    "Your exact prompt here", 
    {}, 
    "analysis", 
    "chat", 
    false  // Disable terse-response instruction
);
```

This option is useful when you need precise control over prompts for evaluation or when integrating with downstream agents that require exact prompt matching.

[↑ Back to top](#llmengine)

---

<a id="documentation"></a>
## 📚 Documentation

| File | Description |
|------|--------------|
| [QUICKSTART.md](QUICKSTART.md) | Getting started guide |
| [docs/CONFIGURATION.md](docs/CONFIGURATION.md) | Configuration structure |
| [docs/PROVIDERS.md](docs/PROVIDERS.md) | Provider details |
| [docs/API_REFERENCE.md](docs/API_REFERENCE.md) | Generated Doxygen API |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | System architecture overview |
| [docs/FAQ.md](docs/FAQ.md) | Frequently Asked Questions |
| [docs/SECURITY.md](docs/SECURITY.md) | Security notes |
| [docs/PERFORMANCE.md](docs/PERFORMANCE.md) | Optimization tips |
| [docs/CI_CD_GUIDE.md](docs/CI_CD_GUIDE.md) | CI/CD and testing guide |
| [.github/CONTRIBUTING.md](.github/CONTRIBUTING.md) | Contribution guidelines |
| [examples/README.md](examples/README.md) | Example usage guide |

[↑ Back to top](#llmengine)

---

<a id="license"></a>
## 📜 License

This project is licensed under the **GNU General Public License v3.0**.  
See the [LICENSE](LICENSE) file for full details.