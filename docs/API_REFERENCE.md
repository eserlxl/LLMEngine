# API Reference (Overview)

This overview links to public headers with Doxygen-style comments.

- `src/LLMEngine.hpp` — Main entry point; constructors, analyze method, provider info
- `src/APIClient.hpp` — Interface and concrete clients for providers
- `src/LLMOutputProcessor.hpp` — Helpers for parsing/normalizing outputs
- `src/Utils.hpp` — Utility helpers used across the library

## Key Types

- `::LLMEngineAPI::ProviderType` — Provider enumeration (QWEN, OPENAI, ANTHROPIC, OLLAMA)

## Typical Usage

```cpp
#include "LLMEngine.hpp"

LLMEngine engine(::LLMEngineAPI::ProviderType::QWEN, apiKey, "qwen-flash");
auto result = engine.analyze(prompt, {}, "tag");
```

For configuration details, see `docs/CONFIGURATION.md` and `config/api_config.json`.

