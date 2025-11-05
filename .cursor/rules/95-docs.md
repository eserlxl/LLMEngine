# Documentation Rules

## Overview

This document defines documentation guidelines for LLMEngine. These rules ensure clear, consistent, and maintainable documentation that helps users and contributors understand the codebase.

## Writing Style

### Third-Person Formal Style

- **Always use third person**: Never use first person ("I", "we") or second person ("you", "your")
- **Formal tone**: Professional, clear, and precise
- **Active voice**: Prefer active voice when possible

**Good**:
```
The Client class provides a unified interface for interacting with LLM providers.
The method processes requests and returns responses.
```

**Bad**:
```
You can use the Client class to interact with LLM providers.
We process requests and return responses.
```

### Documentation Sections

- **Overview**: Brief description of what the component does
- **Usage**: How to use the component
- **API Reference**: Detailed API documentation
- **Examples**: Code examples demonstrating usage
- **Notes**: Important considerations or limitations

## Doxygen Comments

### Public API Documentation

Use Doxygen-style comments for all public APIs:

```cpp
/**
 * @brief Sends a request to the LLM provider.
 *
 * This method constructs a request from the provided prompt and parameters,
 * sends it to the configured provider, and returns the response.
 *
 * @param prompt The input prompt to send to the LLM.
 * @param parameters Optional parameters for the request (temperature, max_tokens, etc.).
 * @return std::expected<Response, Error> The response on success, or an error on failure.
 *
 * @note This method is thread-safe and can be called concurrently.
 * @note The method may block while waiting for the provider's response.
 *
 * @example
 * ```cpp
 * auto result = client.send_request("Hello, world!", {{"temperature", 0.7}});
 * if (result) {
 *   std::cout << result->text() << std::endl;
 * }
 * ```
 */
std::expected<Response, Error> send_request(
  const std::string& prompt,
  const std::map<std::string, std::string>& parameters = {}
);
```

### Doxygen Tags

Common Doxygen tags:

- **@brief**: Brief description (first line)
- **@param**: Parameter documentation
- **@return**: Return value documentation
- **@note**: Important notes
- **@warning**: Warnings
- **@deprecated**: Deprecated functionality
- **@example**: Code examples
- **@see**: Cross-references
- **@since**: Version when added

### Class Documentation

```cpp
/**
 * @brief Main client class for interacting with LLM providers.
 *
 * The Client class provides a unified interface for sending requests to
 * various LLM providers (OpenAI, Anthropic, Ollama, etc.). It handles
 * provider-specific details internally, allowing users to switch providers
 * without changing their code.
 *
 * @example
 * ```cpp
 * Client client(ProviderType::OpenAI, api_key);
 * auto response = client.send_request("Hello!");
 * ```
 */
class Client {
  // ...
};
```

## README Structure

### Standard Sections

1. **Title and Badge**: Project name, license, C++ version
2. **Overview**: Brief description of what the project does
3. **Features**: Key features and capabilities
4. **Quick Start**: Minimal example to get started
5. **Installation**: How to build and install
6. **Usage**: Basic usage examples
7. **Configuration**: Configuration options
8. **API Reference**: Link to detailed API docs
9. **Contributing**: How to contribute
10. **License**: License information

### Example README Structure

```markdown
# LLMEngine

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/std/status)

## Overview

LLMEngine is a modern C++20 library that provides a unified interface
for interacting with Large Language Model providers.

## Features

- Unified API for multiple providers
- Type-safe error handling
- Configuration-driven architecture

## Quick Start

\```cpp
#include "LLMEngine.hpp"

int main() {
  LLMEngine::Client client(ProviderType::OpenAI, api_key);
  auto response = client.send_request("Hello, world!");
  // ...
}
\```

## Installation

[Installation instructions]

## API Reference

See [API_REFERENCE.md](docs/API_REFERENCE.md) for detailed documentation.

## License

GPL-3.0-or-later
```

## CONTRIBUTING Guidelines

### Required Sections

1. **Getting Started**: How to set up development environment
2. **Code Style**: Reference to style guide
3. **Testing**: How to write and run tests
4. **Pull Requests**: PR process and requirements
5. **Commit Messages**: Reference to commit message format

### Example CONTRIBUTING

```markdown
# Contributing to LLMEngine

## Getting Started

[Setup instructions]

## Code Style

Follow the style guide in `.cursor/rules/10-cpp-style.md`.

## Testing

All new code must include tests. See `.cursor/rules/60-tests.md` for guidelines.

## Pull Requests

[PR process]

## Commit Messages

Use Conventional Commits format. See `.cursor/rules/90-commit-and-iteration.md`.
```

## Design Documentation

### Location

- **Design docs**: `docs/` directory
- **Architecture**: `docs/ARCHITECTURE.md`
- **API Reference**: `docs/API_REFERENCE.md`
- **Performance**: `docs/PERFORMANCE.md`

### Design Doc Structure

1. **Overview**: High-level design
2. **Architecture**: System architecture and components
3. **Design Decisions**: Key decisions and rationale
4. **API Design**: Public API design principles
5. **Implementation Notes**: Implementation details

## Code Examples

### Compilable Examples

All code examples must be compilable and runnable:

```cpp
// ✅ Good: Complete, compilable example
#include "LLMEngine.hpp"
#include <iostream>

int main() {
  LLMEngine::Client client(ProviderType::OpenAI, "api-key");
  auto response = client.send_request("Hello!");
  if (response) {
    std::cout << response->text() << std::endl;
  }
  return 0;
}
```

```cpp
// ❌ Bad: Incomplete, missing includes/context
Client client(...);
auto response = client.send_request("Hello!");
```

### Example Requirements

- **Complete**: Include all necessary headers
- **Minimal**: Show only relevant code
- **Runnable**: Examples should compile and run
- **Clear**: Comments explain non-obvious parts

## Documentation Updates

### When to Update Documentation

- **API changes**: Update API documentation
- **New features**: Document new functionality
- **Behavior changes**: Update relevant sections
- **Bug fixes**: Update if behavior changed

### Documentation Checklist

Before committing:

- [ ] Public API documented with Doxygen
- [ ] README updated if needed
- [ ] Examples updated if API changed
- [ ] Design docs updated if architecture changed
- [ ] Third-person formal style used

## Inline Comments

### Code Comments

- **Why, not what**: Explain why code exists, not what it does
- **Complex logic**: Document non-obvious algorithms
- **TODOs**: Use `// TODO: description` for future work
- **FIXMEs**: Use `// FIXME: description` for known issues

```cpp
// Good: Explains why
// Use exponential backoff to avoid overwhelming the server
// with retry requests during outages.
auto delay = calculate_backoff(attempt);

// Bad: States the obvious
// Calculate the backoff delay
auto delay = calculate_backoff(attempt);
```

## Summary

1. **Third-person formal**: Never use first/second person
2. **Doxygen comments**: Document all public APIs
3. **README structure**: Standard sections with examples
4. **CONTRIBUTING**: Clear contribution guidelines
5. **Design docs**: Architecture and design decisions
6. **Compilable examples**: All examples must compile
7. **Update docs**: Keep documentation in sync with code

## References

- See `10-cpp-style.md` for code style
- See `90-commit-and-iteration.md` for commit messages
- [Doxygen Documentation](https://www.doxygen.nl/manual/index.html)

