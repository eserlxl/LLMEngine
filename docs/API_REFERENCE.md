# API Reference

This page is the entry point to the generated Doxygen documentation for LLMEngine's public C++ API.

## Generated HTML (Doxygen)

If your build generates documentation, open the HTML output in your browser. Typical locations:

- After configuring a docs target: `build/docs/html/index.html`
- Or your build system's doc artifact directory

## Generate Locally

If a `docs` target is available in your CMake setup:

```bash
cmake -S . -B build
cmake --build build --target docs
```

Then open:

```bash
xdg-open build/docs/html/index.html
```

If your environment does not define a `docs` target, you can run Doxygen manually using the provided `docs/Doxyfile`.

## Public Headers (reference points)

- `src/LLMEngine.hpp` — Main entry point; constructors, analyze method, provider info, `AnalysisResult`
- `src/APIClient.hpp` — Interface and concrete clients for providers
- `src/LLMOutputProcessor.hpp` — Output parsing/normalization helpers
- `src/Utils.hpp` — Utility helpers used across the library

For configuration details, see `docs/CONFIGURATION.md` and `config/api_config.json`.

## Typed Result

`LLMEngine::analyze` returns `AnalysisResult`:

```0:0:src/LLMEngine.hpp
struct AnalysisResult { bool success; std::string think; std::string content; std::string errorMessage; int statusCode; };
```

Access response text via `result.content`. The previous `std::vector<std::string>` return is removed.

