# Error Handling Rules

## Overview

This document defines error handling patterns and policies for LLMEngine. The goal is to make error paths explicit, testable, and safe while maintaining performance and code clarity.

## RAII-First Approach

### Resource Safety

- **All resources must be managed by RAII**: Files, memory, network connections, locks
- **Never leak resources**: Use smart pointers, containers, or RAII wrappers
- **Exception safety**: Ensure resources are cleaned up even if exceptions are thrown

**Good**:
```cpp
{
  auto file = std::ifstream("data.txt");
  if (!file) {
    return Error::FileNotFound;
  }
  // File automatically closed when scope exits
}
```

**Bad**:
```cpp
FILE* f = fopen("data.txt", "r");
// What if exception is thrown? File leak!
fclose(f);
```

### Constructor/Destructor Discipline

- **Constructors**: Acquire resources, establish invariants
- **Destructors**: Release resources, maintain noexcept
- **No throwing from destructors**: Destructors should be `noexcept`

## Return-Based Error Models

### Prefer std::expected (C++23)

When available, use `std::expected<T, E>` for functions that can fail:

```cpp
#include <expected>

std::expected<Response, Error> send_request(const Request& req) {
  if (req.invalid()) {
    return std::unexpected(Error::InvalidRequest);
  }
  // ... process ...
  return Response{data};
}

// Usage
auto result = send_request(req);
if (result) {
  process(*result);
} else {
  handle_error(result.error());
}
```

### Status Objects (Fallback)

When `std::expected` is unavailable, use status objects:

```cpp
struct Status {
  bool ok;
  std::string message;
};

std::pair<Response, Status> send_request(const Request& req) {
  if (req.invalid()) {
    return {{}, Status{false, "Invalid request"}};
  }
  // ... process ...
  return {Response{data}, Status{true, ""}};
}

// Usage
auto [response, status] = send_request(req);
if (!status.ok) {
  handle_error(status.message);
}
```

### Error Codes

For simple cases, use error codes with optional value:

```cpp
enum class ErrorCode {
  Success,
  InvalidInput,
  NetworkError,
  Timeout
};

std::pair<ErrorCode, std::optional<Response>> send_request(const Request& req);
```

## Exception Policy

### When to Use Exceptions

Exceptions should be used **only for exceptional flows**:

1. **Programming errors**: Assertions, invariant violations
2. **Resource exhaustion**: Out of memory, system limits
3. **Unrecoverable errors**: Invalid state, corrupted data

**Good**:
```cpp
void process(const Config& config) {
  if (!config.is_valid()) {
    throw std::invalid_argument("Invalid configuration");
  }
  // ...
}
```

### When NOT to Use Exceptions

Do **not** use exceptions for:

1. **Expected failures**: Network timeouts, file not found (use return values)
2. **Control flow**: Use `if`/`return` instead
3. **Performance-critical paths**: Prefer return codes

**Bad**:
```cpp
std::string read_file(const std::string& path) {
  std::ifstream file(path);
  if (!file) {
    throw std::runtime_error("File not found"); // Expected failure
  }
  // ...
}
```

**Good**:
```cpp
std::expected<std::string, Error> read_file(const std::string& path) {
  std::ifstream file(path);
  if (!file) {
    return std::unexpected(Error::FileNotFound);
  }
  // ...
}
```

### Never Throw Across Module Boundaries

- **Public APIs**: Prefer return-based error models
- **Internal code**: May use exceptions for exceptional cases
- **Shared library boundaries**: Never throw exceptions across DLL/SO boundaries (ABI issues)

**Reason**: Exceptions across module boundaries can cause:
- ABI compatibility issues
- Undefined behavior if exception types differ
- Linking problems with different exception handling models

## Logging

### Lightweight Logger Interface

- **No global singletons**: Pass logger as dependency
- **Interface-based**: Accept logger interface, not concrete type
- **Structured logging**: Use structured log levels and context

```cpp
class Logger {
public:
  virtual ~Logger() = default;
  virtual void log(LogLevel level, const std::string& message) = 0;
};

class Client {
public:
  explicit Client(std::shared_ptr<Logger> logger) : logger_(std::move(logger)) {}
  
private:
  void handle_error(const Error& err) {
    if (logger_) {
      logger_->log(LogLevel::Error, "Request failed: " + err.message());
    }
  }
  
  std::shared_ptr<Logger> logger_;
};
```

### Log Levels

- **Error**: Unexpected failures, errors requiring attention
- **Warning**: Recoverable issues, deprecation notices
- **Info**: Important state changes, high-level operations
- **Debug**: Detailed information for debugging (compile-time disabled in release)

### Avoid Logging in Hot Paths

- Minimize logging in performance-critical code
- Use conditional compilation for debug logs
- Consider async logging for non-critical paths

## Error Propagation Patterns

### Return and Propagate

```cpp
std::expected<Data, Error> fetch_data() {
  auto result = send_request(req);
  if (!result) {
    return result; // Propagate error
  }
  return parse(*result);
}
```

### Error Transformation

```cpp
std::expected<Data, Error> fetch_data() {
  auto result = send_request(req);
  if (!result) {
    return std::unexpected(transform_error(result.error()));
  }
  return parse(*result);
}
```

### Error Recovery

```cpp
std::expected<Data, Error> fetch_data_with_retry() {
  for (int i = 0; i < MAX_RETRIES; ++i) {
    auto result = send_request(req);
    if (result) {
      return parse(*result);
    }
    if (!is_retryable(result.error())) {
      return result; // Non-retryable error
    }
    // Retry logic
  }
  return std::unexpected(Error::MaxRetriesExceeded);
}
```

## Testing Error Paths

- **All error paths must be tested**: Unit tests for failure modes
- **Mock dependencies**: Use mocks to simulate failures
- **Error injection**: Test error handling, not just success paths

See `60-tests.md` for testing guidelines.

## Summary

1. **RAII first**: All resources managed automatically
2. **Return-based errors**: Use `std::expected` or status objects for expected failures
3. **Exceptions sparingly**: Only for truly exceptional cases
4. **No exceptions across modules**: Never throw across DLL/SO boundaries
5. **Logging**: Lightweight, interface-based, no global singletons
6. **Test error paths**: All failure modes must be tested

## References

- See `00-architecture.md` for module boundaries
- See `60-tests.md` for error path testing
- See `30-performance.md` for performance considerations

