# API Reference

This page is the entry point to the generated Doxygen documentation for LLMEngine's public C++ API.

## Generated HTML (Doxygen)

If the build generates documentation, open the HTML output in a browser. Typical locations:

- After configuring a docs target: `build/docs/html/index.html`
- Or the build system's doc artifact directory

## Generate Locally

If a `docs` target is available in the CMake setup:

```bash
cmake -S . -B build
cmake --build build --target docs
```

Then open:

```bash
xdg-open build/docs/html/index.html
```

If the environment does not define a `docs` target, Doxygen can be run manually using the provided `docs/Doxyfile`.

## Public Headers

### Preferred Include Paths

The library uses a namespaced include structure under `LLMEngine/`. **These are the recommended include paths:**

```cpp
#include "LLMEngine/LLMEngine.hpp"          // Main entry point
#include "LLMEngine/APIClient.hpp"          // Provider interface and clients
#include "LLMEngine/Utils.hpp"              // Utility functions
#include "LLMEngine/DebugArtifacts.hpp"     // Debug artifact helpers
```

**Additional headers:**
```cpp
#include "LLMEngine/Logger.hpp"             // Logger interface
#include "LLMEngine/RequestLogger.hpp"      // Request logging utilities
```

### Legacy Compatibility Headers

The following compatibility headers are provided in the root `include/` directory for backward compatibility but are **deprecated** and may be removed in a future version:

- `include/LLMEngine.hpp` → redirects to `LLMEngine/LLMEngine.hpp`
- `include/APIClient.hpp` → redirects to `LLMEngine/APIClient.hpp`
- `include/Utils.hpp` → redirects to `LLMEngine/Utils.hpp`
- `include/Logger.hpp` → redirects to `LLMEngine/Logger.hpp`
- `include/RequestLogger.hpp` → redirects to `LLMEngine/RequestLogger.hpp`
- `include/DebugArtifacts.hpp` → redirects to `LLMEngine/DebugArtifacts.hpp`

The former `LLMOutputProcessor` API has been removed. Consumers should parse responses using `LLMEngine::ResponseParser` or their own logic.

For configuration details, see [docs/configuration.md](configuration.md) and `config/api_config.json`.

## See Also

- [docs/configuration.md](configuration.md) - Configuration setup
- [docs/providers.md](providers.md) - Provider details
- [docs/architecture.md](architecture.md) - Architecture overview
- [quickstart.md](../quickstart.md) - Quick start examples
- [examples/README.md](../examples/README.md) - Code examples
- [README.md](../README.md) - Main documentation

## Typed Result

`LLMEngine::analyze` returns `AnalysisResult`:

```0:0:src/LLMEngine.hpp
struct AnalysisResult { bool success; std::string think; std::string content; std::string errorMessage; int statusCode; };
```

Access response text via `result.content`. The previous `std::vector<std::string>` return is removed.

