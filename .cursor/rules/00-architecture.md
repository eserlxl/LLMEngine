# Architecture Rules

## Overview

This document defines the architectural principles and patterns for LLMEngine. These rules ensure consistent code organization, clear module boundaries, and maintainable dependencies.

## Directory Structure and Layering

### Public API Layer (`include/LLMEngine/`)

- **Purpose**: Public headers that form the stable API surface
- **Naming**: PascalCase for classes, namespaced under `LLMEngine`
- **Visibility**: All headers in this directory are public and may be installed
- **Stability**: Breaking changes require version bump and migration guide
- **Dependencies**: Only standard library and well-defined third-party types (hidden behind interfaces)

### Internal Implementation Layer (`src/`)

- **Purpose**: Implementation details, not exposed to consumers
- **Naming**: One class per file, matching header name
- **Visibility**: Internal only; headers may exist in `src/` but are not installed
- **Dependencies**: Can depend on internal helpers, third-party implementations

### Test Layer (`test/`)

- **Purpose**: Unit tests, integration tests, and test support code
- **Naming**: `test_*.cpp` for unit tests, `test_*_integration.cpp` for integration tests
- **Dependencies**: Can access both public and internal APIs for testing
- **Structure**: Mirror source directory structure where possible

### Examples Layer (`examples/`)

- **Purpose**: Example programs demonstrating library usage
- **Dependencies**: Only public API (`include/LLMEngine/`)
- **Build**: Optional, controlled by `ENABLE_EXAMPLES` CMake option

## Module Boundaries

### Public API vs Internal Details

- **Public headers** (`include/LLMEngine/`) must not expose:
  - Implementation details (private members visible in headers)
  - Third-party library types directly (use forward declarations or abstractions)
  - Internal helper classes or utilities

- **Internal code** (`src/`) may:
  - Use implementation details freely
  - Depend on third-party libraries directly
  - Include internal headers from other internal modules

### ABI Stability Guidelines

When building shared libraries (`BUILD_SHARED_LIBS=ON`):

1. **Public headers** must maintain ABI compatibility across minor versions
2. **Virtual functions**: Adding new virtual functions breaks ABI (use composition or extension points)
3. **Data members**: Adding data members to public classes breaks ABI (use pImpl)
4. **Template parameters**: Changes to template parameters may break ABI
5. **Export symbols**: Use visibility macros (`LLMENGINE_API`) for shared library exports

### pImpl Pattern Usage

Use pImpl when:

- **ABI stability required**: Public classes in shared libraries that need to evolve
- **Compilation firewalls**: Reduce compilation dependencies (e.g., large third-party headers)
- **Binary compatibility**: Hide implementation details that may change

**Example**:

```cpp
// Public header (include/LLMEngine/Client.hpp)
namespace LLMEngine {
class LLMENGINE_API Client {
public:
  Client();
  ~Client();
  void process();
  
private:
  class Impl;
  std::unique_ptr<Impl> pimpl_;
};
}
```

**Avoid pImpl when**:

- Simple value types or stateless classes
- Performance-critical code where indirection overhead matters
- Template classes (pImpl doesn't work with templates)

## Dependency Rules

### Standard Library First

- Prefer standard library (`std::`) over third-party libraries
- Use `std::string`, `std::vector`, `std::optional`, `std::variant`, etc.
- Leverage C++20/23 features: `std::span`, `std::ranges`, `std::expected` (when available)

### Third-Party Isolation

- **Public headers must not leak third-party types**: Use forward declarations, abstractions, or opaque types
- **Internal code may use third-party libraries directly**: But prefer interfaces
- **Dependency injection**: Accept interfaces rather than concrete types when possible

**Good**: Public API accepts `std::string`, not `nlohmann::json`
**Bad**: Public API exposes `nlohmann::json` directly (leaks dependency)

### Dependency Management

- **CMake**: Use `FetchContent` for dependencies when system packages unavailable
- **System packages**: Prefer system-provided packages when available (via `find_package`)
- **Version pinning**: Pin third-party versions for reproducible builds
- **Optional dependencies**: Gate behind CMake options when appropriate

## Header Hygiene

### Include Order

1. **Corresponding header**: `#include "Client.hpp"` in `Client.cpp`
2. **System headers**: `#include <string>`, `#include <vector>`
3. **Third-party headers**: `#include <nlohmann/json.hpp>`
4. **Project headers**: `#include "LLMEngine/Utils.hpp"`

### Forward Declarations

- Use forward declarations in headers when possible
- Include full definitions only in implementation files
- Reduces compilation time and coupling

**Example**:

```cpp
// Header
namespace nlohmann { template<typename T> class json; }
class Client {
  void process(const nlohmann::json& config);
};

// Implementation
#include <nlohmann/json.hpp>
void Client::process(const nlohmann::json& config) { /* ... */ }
```

### Include Guards

- Use `#pragma once` (modern, supported by all major compilers)
- Alternative: `#ifndef` guards if `#pragma once` unavailable

### Minimal Includes

- Include only what is necessary
- Avoid transitive includes (e.g., don't include `<iostream>` just for `std::string`)
- Use `std::string_view` for read-only string parameters to avoid `#include <string>`

## Namespace Organization

- **Public API**: `namespace LLMEngine { ... }`
- **Internal implementation**: `namespace LLMEngine::detail { ... }` or `namespace LLMEngine::internal { ... }`
- **Test support**: `namespace LLMEngine::test { ... }` or separate test namespace

## Module Design Principles

1. **Single Responsibility**: Each module/class has one clear purpose
2. **High Cohesion**: Related functionality grouped together
3. **Low Coupling**: Minimize dependencies between modules
4. **Explicit Interfaces**: Clear contracts for public APIs
5. **Dependency Inversion**: Depend on abstractions, not concretions

## Cross-Cutting Concerns

- **Logging**: Lightweight logger interface, no global singletons
- **Error Handling**: Return-based error models (see `20-error-handling.md`)
- **Configuration**: JSON-based config, isolated from public API
- **Testing**: Mockable interfaces for external dependencies

## References

- See `10-cpp-style.md` for code style details
- See `20-error-handling.md` for error handling patterns
- See `50-cmake.md` for CMake structure and dependency management

