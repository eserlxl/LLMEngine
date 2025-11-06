# LLMEngine

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/std/status)
[![CI](https://github.com/eserlxl/LLMEngine/workflows/LLMEngine%20CI/badge.svg)](https://github.com/eserlxl/LLMEngine/actions)
[![Tests](https://img.shields.io/badge/tests-passing-brightgreen)](https://github.com/eserlxl/LLMEngine/actions)

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

- C++20 compiler (e.g., GCC 11+, Clang 14+)  
- CMake 3.20+  
- Dependencies: **OpenSSL**, **nlohmann_json**, **cpr**

**Note:** Windows is not officially supported. While the codebase includes partial MSVC support in the build system, Windows has known limitations:

- **Temporary directories**: The default temporary directory path is hardcoded to Unix-style paths (`/tmp/llmengine`). Custom `ITempDirProvider` implementations can be used to provide Windows-compatible paths.
- **Command execution**: The `execCommand()` function is disabled on Windows due to POSIX dependencies (`posix_spawn` API). The function will return an empty result set with an error log when called on Windows. For Windows compatibility, consider using alternative command execution methods or the vector-based `execCommand()` overload with a custom implementation.
- **Other limitations**: Some POSIX-specific features may not work correctly on Windows. Testing and contributions for Windows support are welcome.

### Minimal Example

```cpp
#include "LLMEngine/LLMEngine.hpp"
#include <iostream>

using namespace LLMEngine;

int main() {
    const char* api_key = std::getenv("QWEN_API_KEY");
    LLMEngine engine(::LLMEngineAPI::ProviderType::QWEN, api_key, "qwen-flash");
    auto result = engine.analyze("Explain quantum computing simply:", {}, "demo");
    std::cout << result.content << std::endl;
    return 0;
}
```

[↑ Back to top](#llmengine)

---

<a id="installation"></a>
## 🧱 Installation

Clone the repository and build:

```bash
git clone https://github.com/eserlxl/LLMEngine
cd LLMEngine
./build.sh
```

### Building from Source

The project uses CMake with several build configuration options. Key options include:

- **`DEBUG_MODE`** (OFF): Enable debug build flags (`-O0 -g`)
- **`PERFORMANCE_BUILD`** (OFF): Enable performance-optimized flags (`-O3`)
  - **Note:** `DEBUG_MODE` and `PERFORMANCE_BUILD` are mutually exclusive
- **`BUILD_TESTING`** (ON): Build tests and enable CTest
- **`ENABLE_EXAMPLES`** (ON): Build example programs
- **`WARNING_MODE`** (ON): Enable extra compiler warnings
- **`ENABLE_SANITIZERS`** (OFF): Enable Address/Undefined sanitizers in debug builds
- **`LLM_DISABLE_FETCHCONTENT`** (OFF): Disable FetchContent; require system packages

Example build configurations:

```bash
# Debug build with sanitizers
cmake -S . -B build_debug -DDEBUG_MODE=ON -DENABLE_SANITIZERS=ON
cmake --build build_debug -j20

# Performance-optimized build
cmake -S . -B build_perf -DPERFORMANCE_BUILD=ON -DENABLE_NATIVE_OPTIMIZATION=ON
cmake --build build_perf -j20

# Release build (default)
cmake -S . -B build_release
cmake --build build_release -j20
```

For detailed build instructions, dependencies, and all build options, see [docs/BUILD.md](docs/BUILD.md).

[↑ Back to top](#llmengine)

---

<a id="usage"></a>
## 💻 Usage

### Provider Initialization

```cpp
#include "LLMEngine/LLMEngine.hpp"
using namespace LLMEngine;

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
using namespace LLMEngine;

LLMEngine engine("qwen", api_key, "qwen-flash");
LLMEngine engine2("qwen", api_key);  // Uses default model from config
```

### Custom Parameters

```cpp
using namespace LLMEngine;

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
using namespace LLMEngine;

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

## 🧵 Concurrency Model

- LLMEngine instances are not thread-safe; use one instance per thread.
- Provider clients are stateless and thread-safe, but are owned by each LLMEngine instance.
- If sharing a logger across threads, ensure the logger implementation is thread-safe.

[↑ Back to top](#llmengine)

---

## 🛡️ Production Considerations

- Credentials: provide via environment variables (e.g., `QWEN_API_KEY`, `OPENAI_API_KEY`), not config files.
- Debug artifacts: disable in production by setting `LLMENGINE_DISABLE_DEBUG_FILES` or inject a policy via `setDebugFilesPolicy`.
- Temporary files: default under `/tmp/llmengine`; directories are secured (no symlinks, owner-only perms). Use an `ITempDirProvider` to customize.
- Retries: use a capped retry policy with jitter for transient errors; avoid retrying on auth/4xx.
- Build: enable warnings and hardening flags; for shared builds, consider hidden symbol visibility.
- CI: run sanitizer-enabled Debug builds and optimized Release builds; exercise Linux GCC/Clang (and optionally macOS).

[↑ Back to top](#llmengine)

---

## 🧪 Testing

Tests are enabled by default when `BUILD_TESTING=ON`.

Build and run tests:

```bash
cmake -S . -B build_test -DBUILD_TESTING=ON
cmake --build build_test -j20
ctest --test-dir build_test --output-on-failure
```

Environment variables used by tests (set if hitting online providers):

- `QWEN_API_KEY`, `OPENAI_API_KEY`, `ANTHROPIC_API_KEY`, `GEMINI_API_KEY`
- Optional toggles: `LLMENGINE_DISABLE_DEBUG_FILES` (disable artifact writes), `LLMENGINE_LOG_REQUESTS` (prints sanitized requests)

Some tests will be skipped automatically if the corresponding API keys are not present.

`/tmp` is used for temporary artifacts; paths can be overridden via a custom `ITempDirProvider` in tests.

---

## 🔧 Build Configuration

For detailed information about build presets, CMake options, and build configurations, see [docs/BUILD.md](docs/BUILD.md).

[↑ Back to top](#llmengine)

---

<a id="documentation"></a>
## 📚 Documentation

| File | Description |
|------|--------------|
| [QUICKSTART.md](QUICKSTART.md) | Getting started guide |
| [docs/BUILD.md](docs/BUILD.md) | Build presets and CMake options |
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