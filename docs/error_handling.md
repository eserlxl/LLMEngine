# Error Handling Guidelines

This document describes the error handling patterns and guidelines used in LLMEngine.

## Overview

LLMEngine uses a hybrid error handling approach that balances safety, performance, and clarity:

- **Result<T, E>** for expected failures in internal functions
- **Exceptions** for programming errors and resource exhaustion
- **AnalysisResult** struct for public API (backward compatibility)

## Error Handling Patterns

### Result<T, E> for Expected Failures

Use `Result<T, E>` (or the convenience alias `ResultOrError<T>`) for functions that can fail in expected ways:

```cpp
#include "LLMEngine/Result.hpp"
#include "LLMEngine/ResultError.hpp"

ResultOrError<std::string> parseConfig(const std::string& path) {
    if (path.empty()) {
        return Result<std::string, ResultError>::err(
            ResultError(LLMEngineErrorCode::Client, "Path cannot be empty"));
    }
    // ... parse ...
    return Result<std::string, ResultError>::ok(parsed_value);
}

// Usage
auto result = parseConfig(path);
if (result) {
    process(result.value());
} else {
    handle_error(result.error().code, result.error().message);
}
```

**When to use Result<T, E>:**
- Network errors (timeouts, connection failures)
- Invalid input validation
- Configuration errors
- File I/O errors (expected failures)
- Internal utility functions
- Non-throwing APIs

### Exceptions for Exceptional Cases

Reserve exceptions for truly exceptional cases:

```cpp
void process(const Config& config) {
    if (!config.is_valid()) {
        throw std::invalid_argument("Invalid configuration");
    }
    // ...
}
```

**When to use exceptions:**
- Programming errors (null pointer dereference, invalid state)
- Resource exhaustion (out of memory, too many open files)
- Constructor failures (when object construction cannot proceed)
- Truly exceptional cases that should never happen in correct code

**When NOT to use exceptions:**
- Expected failures (use Result instead)
- Control flow (use if/return instead)
- Performance-critical paths (prefer return codes)

### Public API: AnalysisResult

The public API (`LLMEngine::analyze()`) uses `AnalysisResult` struct for backward compatibility:

```cpp
struct AnalysisResult {
    bool success;
    std::string think;
    std::string content;
    std::string errorMessage;
    int statusCode;
    LLMEngineErrorCode errorCode;
};
```

This may be migrated to `Result` in a future major version.

## Error Codes

LLMEngine uses `LLMEngineErrorCode` enum for structured error classification:

```cpp
enum class LLMEngineErrorCode {
    Unknown,
    Client,      // Client-side errors (invalid input, configuration)
    Server,      // Server-side errors (5xx responses)
    Network,     // Network errors (timeouts, connection failures)
    Auth,        // Authentication errors (401, 403)
    RateLimited, // Rate limiting (429)
    InvalidResponse, // Invalid response format
    // ...
};
```

## Best Practices

### 1. Never Throw Across Module Boundaries

Exceptions should not cross shared library boundaries (DLL/SO) due to ABI compatibility issues.

### 2. RAII for Resource Management

Always use RAII for resource management:

```cpp
{
    auto file = std::ifstream("data.txt");
    if (!file) {
        return Result<std::string, ResultError>::err(
            ResultError(LLMEngineErrorCode::Client, "File not found"));
    }
    // File automatically closed when scope exits
}
```

### 3. No Exceptions in Destructors

Destructors should be `noexcept` and never throw exceptions.

### 4. Empty Catch Blocks

Empty catch blocks should be documented with clear rationale:

```cpp
try {
    // ... operation ...
} catch (const std::exception& e) {  // NOLINT(bugprone-empty-catch)
    // Best-effort operation: failures are non-fatal.
    // This is expected behavior in some environments.
    // Logging is not available in this context.
    (void)e;  // Suppress unused variable warning
}
```

### 5. Error Message Quality

Error messages should be:
- **Descriptive**: Explain what went wrong
- **Contextual**: Include relevant context (file path, HTTP status, etc.)
- **Actionable**: Suggest how to fix the issue when possible

## Migration Strategy

The codebase is gradually migrating to `Result<T, E>`:

1. **New internal functions**: Use `Result<T, E>` or `ResultOrError<T>`
2. **Existing functions**: Gradually migrate to `Result` where appropriate
3. **Public API**: Keep `AnalysisResult` for backward compatibility
4. **Constructors**: Continue using exceptions for construction failures

## Examples

### Good: Using Result for Expected Failures

```cpp
ResultOrError<Config> loadConfig(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        return Result<Config, ResultError>::err(
            ResultError(LLMEngineErrorCode::Client, 
                       "Failed to open config file: " + path));
    }
    // ... parse ...
    return Result<Config, ResultError>::ok(config);
}
```

### Good: Using Exceptions for Programming Errors

```cpp
void setApiKey(const std::string& key) {
    if (key.empty()) {
        throw std::invalid_argument("API key cannot be empty");
    }
    api_key_ = key;
}
```

### Bad: Using Exceptions for Expected Failures

```cpp
// ❌ Bad: File not found is an expected failure
std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("File not found");  // Should use Result
    }
    // ...
}
```

## References

- See `.cursor/rules/20-error-handling.md` for detailed error handling rules
- See `include/LLMEngine/Result.hpp` for Result type implementation
- See `include/LLMEngine/ResultError.hpp` for ResultError type
- See `include/LLMEngine/ErrorCodes.hpp` for error code definitions

