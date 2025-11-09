# C++ Style Guide

## Overview

This document defines the coding style and conventions for LLMEngine. These rules ensure consistency, readability, and alignment with modern C++ best practices.

## Naming Conventions

### Classes and Types

- **Classes**: PascalCase
  ```cpp
  class APIClient { };
  class ResponseParser { };
  ```

- **Structs**: PascalCase (prefer `class` for data with invariants)
  ```cpp
  struct Config { };
  ```

- **Type Aliases**: PascalCase
  ```cpp
  using StringList = std::vector<std::string>;
  using Callback = std::function<void()>;
  ```

- **Enums**: PascalCase (prefer `enum class` over `enum`)
  ```cpp
  enum class ProviderType { OpenAI, Anthropic, Ollama };
  ```

### Functions and Variables

- **Functions**: snake_case
  ```cpp
  void process_request();
  std::string get_model_name();
  ```

- **Member Variables**: snake_case, trailing underscore for private members (optional)
  ```cpp
  class Client {
  private:
    std::string api_key_;
    int timeout_seconds_;
  };
  ```

- **Local Variables**: snake_case
  ```cpp
  auto response_data = parse_json(json_str);
  ```

- **Parameters**: snake_case
  ```cpp
  void send_request(const std::string& endpoint, int timeout);
  ```

### Constants

- **Constants**: UPPER_SNAKE_CASE
  ```cpp
  constexpr int MAX_RETRIES = 3;
  constexpr const char* DEFAULT_ENDPOINT = "https://api.example.com";
  ```

- **Static Constants**: UPPER_SNAKE_CASE or camelCase (match class style)
  ```cpp
  class Config {
    static constexpr int MAX_SIZE = 1024;
  };
  ```

### Templates

- **Template Parameters**: PascalCase (single uppercase letter for simple cases)
  ```cpp
  template<typename T>
  class Container { };
  
  template<typename Key, typename Value>
  class Map { };
  ```

### Namespaces

- **Namespaces**: PascalCase (match project name)
  ```cpp
  namespace LLMEngine { }
  namespace LLMEngine::detail { }
  ```

## Header and Source Structure

### One Class Per File

- Each class should have its own header and source file
- File names match class names (PascalCase for headers)
- Example: `Client.hpp` / `Client.cpp` for `class Client`

### Header File Structure

```cpp
// Copyright notice and license
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <system/headers>
#include <third-party/headers>
#include "project/headers"

namespace LLMEngine {

// Forward declarations

// Class definition
class MyClass {
public:
  // Public interface
private:
  // Private members
};

} // namespace LLMEngine
```

### Source File Structure

```cpp
// Copyright notice and license
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MyClass.hpp"

// Additional includes if needed

namespace LLMEngine {

// Implementation

} // namespace LLMEngine
```

## Include Hygiene

### Include Order

1. **Corresponding header** (in `.cpp` files)
   ```cpp
   #include "Client.hpp"
   ```

2. **System headers** (C++ standard library, C headers)
   ```cpp
   #include <string>
   #include <vector>
   #include <cstdint>
   ```

3. **Third-party headers**
   ```cpp
   #include <nlohmann/json.hpp>
   ```

4. **Project headers**
   ```cpp
   #include "LLMEngine/Utils.hpp"
   ```

### Include Guards

- Use `#pragma once` (preferred, modern)
- All headers must have include guards

### Forward Declarations

- Prefer forward declarations in headers
- Include full definitions only when necessary
- Reduces compilation time and coupling

```cpp
// Header
namespace nlohmann { template<typename T> class json; }
class Client {
  void process(const nlohmann::json& config);
};

// Source
#include <nlohmann/json.hpp>
```

## C++ Core Guidelines Alignment

Follow the [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/) with emphasis on:

- **C.1**: Organize related data into structures
- **C.2**: Use `class` if the class has an invariant; use `struct` if the data members can vary independently
- **C.9**: Minimize exposure of members
- **C.21**: If you define or `=delete` any default operation, define or `=delete` them all
- **ES.23**: Prefer the `{}` initializer syntax
- **ES.64**: Use the `T{e}` notation for construction
- **F.15**: Prefer simple and conventional ways of passing information
- **F.21**: To return multiple "out" values, prefer returning a struct or tuple
- **I.1**: Make interfaces explicit
- **I.2**: Avoid non-const global variables
- **R.1**: Manage resources automatically using RAII
- **R.3**: A raw pointer is non-owning
- **R.5**: Prefer scoped objects, don't heap-allocate unnecessarily

## Modern C++ Features

### Constexpr and Const

- Use `constexpr` for compile-time constants
- Use `const` for runtime constants
- Prefer `const` by default, remove only when mutability is needed

```cpp
constexpr int MAX_SIZE = 1024;
const std::string api_version = "v1";
```

### Noexcept

- Mark functions `noexcept` when they cannot throw
- Use `noexcept` for move constructors/assignments when appropriate
- Use `noexcept` for destructors (unless base class virtual destructor)

```cpp
void swap(MyClass& other) noexcept;
MyClass(MyClass&& other) noexcept = default;
~MyClass() noexcept = default;
```

### [[nodiscard]]

- Use `[[nodiscard]]` for functions whose return value should not be ignored
- Use for pure functions, error codes, resource handles

```cpp
[[nodiscard]] bool is_valid() const;
[[nodiscard]] std::unique_ptr<Resource> acquire();
```

### Type Safety

#### std::span

- Use `std::span` for read-only contiguous sequences
- Prefer over raw pointers or `std::vector` parameters

```cpp
void process_data(std::span<const uint8_t> data);
```

#### Ranges (C++20)

- Use `std::ranges` for algorithms when available
- Prefer range-based `for` loops
- Leverage range adaptors for functional-style operations

```cpp
std::ranges::sort(vec);
for (const auto& item : container) { }

// Range adaptors
auto even_numbers = vec | std::views::filter([](int n) { return n % 2 == 0; });
```

#### Concepts (C++20)

- Use concepts to constrain templates
- Define custom concepts for reusable constraints
- Improves error messages and code clarity

```cpp
template<std::integral T>
void process_integer(T value);

template<typename T>
concept StringLike = std::convertible_to<T, std::string_view>;

template<StringLike T>
void process_string(T str);
```

#### Strong Typedefs

- Use `enum class` for type-safe enumerations
- Consider strong typedefs for type safety (e.g., `using UserId = int`)

```cpp
enum class Status { Ok, Error, Pending };
using UserId = int; // Better: struct UserId { int value; };
```

### Filesystem and Time APIs

- Use `std::filesystem` for file operations
- Use `std::chrono` for time handling

```cpp
namespace fs = std::filesystem;
auto path = fs::current_path() / "data.txt";

using namespace std::chrono;
auto duration = 100ms;
auto now = system_clock::now();
```

## RAII and Memory Management

### Zero Raw new/delete

- Never use raw `new`/`delete`
- Use smart pointers: `std::unique_ptr`, `std::shared_ptr`
- Use containers: `std::vector`, `std::string`, etc.

**Bad**:
```cpp
int* ptr = new int(42);
delete ptr;
```

**Good**:
```cpp
auto ptr = std::make_unique<int>(42);
// Automatic cleanup
```

### Smart Pointers

- **`std::unique_ptr`**: Single ownership, default choice
- **`std::shared_ptr`**: Shared ownership, use sparingly
- **`std::weak_ptr`**: Break circular references with `shared_ptr`

```cpp
auto resource = std::make_unique<Resource>();
auto shared = std::make_shared<Resource>();
```

### Rule of Zero / Three / Five

- Prefer **Rule of Zero**: Let compiler generate special members
- If custom destructor needed, define all five (Rule of Five):
  - Destructor
  - Copy constructor
  - Copy assignment
  - Move constructor
  - Move assignment

```cpp
// Rule of Zero (preferred)
class MyClass {
  std::unique_ptr<Impl> impl_;
  // Compiler generates all special members correctly
};

// Rule of Five (if needed)
class MyClass {
  ~MyClass();
  MyClass(const MyClass&);
  MyClass& operator=(const MyClass&);
  MyClass(MyClass&&) noexcept;
  MyClass& operator=(MyClass&&) noexcept;
};
```

## Default and Delete

- Use `= default` to explicitly request compiler-generated functions
- Use `= delete` to prevent unwanted operations

```cpp
class NonCopyable {
public:
  NonCopyable() = default;
  NonCopyable(const NonCopyable&) = delete;
  NonCopyable& operator=(const NonCopyable&) = delete;
  NonCopyable(NonCopyable&&) = default;
  NonCopyable& operator=(NonCopyable&&) = default;
};
```

## Code Formatting

- Follow `.clang-format` configuration
- Run formatter on save in IDE
- See `80-formatting.md` for details

## References

- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/)
- See `00-architecture.md` for header organization
- See `80-formatting.md` for formatting rules

