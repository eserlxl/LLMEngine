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
The LLMEngine class provides a unified interface for interacting with LLM providers.
The analyze() method processes requests and returns structured results.
```

**Bad**:
```
You can use the LLMEngine class to interact with LLM providers.
We process requests and return results.
```

### Documentation Sections

- **Overview**: Brief description of what the component does
- **Usage**: How to use the component
- **API Reference**: Detailed API documentation
- **Examples**: Code examples demonstrating usage
- **Notes**: Important considerations or limitations

## File Naming Conventions

### Documentation Files

All documentation files (`.md` files) must follow the `lowercase_with_underscores` naming convention:

- **Format**: `lowercase_with_underscores.md`
- **Examples**:
  - ✅ `quickstart.md`
  - ✅ `api_reference.md`
  - ✅ `code_of_conduct.md`
  - ✅ `custom_config_path.md`
  - ✅ `error_handling.md`
  - ❌ `QUICKSTART.md`
  - ❌ `API_REFERENCE.md`
  - ❌ `api-reference.md` (use underscores, not hyphens)
  - ❌ `Api Reference.md` (no spaces)

### Special Files (Exceptions)

- **README.md**: Always uppercase (standard convention - this is the **only exception**)
- **LICENSE**: Uppercase, no extension (standard convention)
- **VERSION**: Uppercase, no extension (project convention)

### Scope

This convention applies to **all documentation files** in the project:
- Root-level documentation files (e.g., `quickstart.md`, `changelog.md`, `contributing.md`, `code_of_conduct.md`, `support.md`, `rules.md`)
- Files in the `docs/` directory (e.g., `docs/architecture.md`, `docs/api_reference.md`, `docs/configuration.md`)
- Files in subdirectories of `docs/` (e.g., `docs/nextVersion/versioning.md`, `docs/nextVersion/tag_management.md`)
- All markdown documentation files regardless of location
- The **only exception** is `README.md` which stays uppercase (standard open-source convention)

### Non-Markdown Files in examples/

- Files in `examples/` directory may retain uppercase (e.g., `examples/README.md`) if needed for demonstration purposes

### Rationale

- **Consistency**: Uniform naming convention across all documentation
- **Case-insensitive filesystems**: Prevents issues on Windows/macOS where case-insensitive filesystems might cause conflicts
- **Easier to type**: No need to remember which words are capitalized
- **Modern convention**: Aligns with modern documentation practices in many projects
- **URL-friendly**: Works well in web URLs without special encoding

### When Creating New Documentation

- Always use lowercase_with_underscores for new markdown files
- If referencing external documentation that uses different conventions, that's acceptable in links
- When moving or renaming documentation files, update all references throughout the codebase

## Doxygen Comments

### Public API Documentation

Use Doxygen-style comments for all public APIs:

```cpp
/**
 * @brief Analyzes text using the configured LLM provider.
 *
 * This method sends the provided prompt and input to the LLM provider
 * and returns a structured result containing the response content,
 * reasoning (if available), and error information.
 *
 * @param prompt The prompt or query to send to the LLM.
 * @param input Additional JSON input data (optional).
 * @param analysis_type Type of analysis being performed (used for logging).
 * @param response_type Expected response format ("json" or "chat").
 * @param prepend_terse_instruction Whether to prepend a terse response instruction.
 * @return AnalysisResult containing the response or error details.
 *
 * @note This method may block while waiting for the provider's response.
 * @note The method is not thread-safe; use separate instances per thread.
 */
AnalysisResult analyze(
    const std::string& prompt,
    const nlohmann::json& input = {},
    const std::string& analysis_type = "general",
    const std::string& response_type = "chat",
    bool prepend_terse_instruction = true
);
```

### Doxygen Tags

Common Doxygen tags:

- **@brief**: Brief description (first line)
- **@param**: Parameter documentation
- **@return**: Return value documentation
- **@note**: Important notes
- **@warning**: Warnings about potential issues
- **@deprecated**: Deprecated functionality
- **@example**: Code examples
- **@see**: Cross-references to related functions/classes
- **@since**: Version when feature was added
- **@throws**: Exceptions that may be thrown

### Class Documentation

```cpp
/**
 * @brief Unified interface for interacting with multiple LLM providers.
 *
 * The LLMEngine class provides a consistent API for sending requests to
 * various LLM providers (Qwen, OpenAI, Anthropic, Gemini, Ollama). It handles
 * provider-specific details internally, allowing users to switch providers
 * without changing their code.
 *
 * Thread Safety: Not thread-safe; use separate instances per thread.
 *
 * @example
 * ```cpp
 * LLMEngine engine(ProviderType::QWEN, api_key, "qwen-flash");
 * auto result = engine.analyze("Explain quantum computing:", {}, "demo");
 * if (result.success) {
 *   std::cout << result.content << std::endl;
 * }
 * ```
 */
class LLMEngine {
  // ...
};
```

## README Structure

### Standard Sections

1. **Title and Badges**: Project name, license, C++ version, CI status
2. **Architecture Overview**: High-level system diagram
3. **Key Highlights**: Main features and benefits
4. **Contents**: Table of contents with anchors
5. **Overview**: Brief description of what the project does
6. **Features**: Key features and capabilities
7. **Quick Start**: Minimal example to get started
8. **Installation**: Build instructions and dependencies
9. **Usage**: Basic usage examples
10. **Configuration**: Configuration options and examples
11. **API Keys**: How to set up authentication
12. **Testing**: How to run tests
13. **Documentation**: Links to detailed documentation
14. **License**: License information

## CONTRIBUTING Guidelines

### Required Sections

1. **Getting Started**: How to set up development environment
2. **Code Style**: Reference to style guide
3. **Testing**: How to write and run tests
4. **Pull Requests**: PR process and requirements
5. **Commit Messages**: Commit message conventions

## Design Documentation

### Location

- **Design docs**: `docs/` directory
- **Architecture**: `docs/architecture.md`
- **API Reference**: `docs/api_reference.md`
- **Configuration**: `docs/configuration.md`
- **Security**: `docs/security.md`
- **Performance**: `docs/performance.md`

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

using namespace LLMEngine;

int main() {
    const char* api_key = std::getenv("QWEN_API_KEY");
    LLMEngine engine(::LLMEngineAPI::ProviderType::QWEN, api_key, "qwen-flash");
    
    auto result = engine.analyze("What is 2+2?", {}, "math");
    if (result.success) {
        std::cout << "Answer: " << result.content << std::endl;
    }
    
    return 0;
}
```

```cpp
// ❌ Bad: Incomplete, missing includes/context
LLMEngine engine(...);
auto result = engine.analyze("prompt");
```

### Example Requirements

- **Complete**: Include all necessary headers
- **Minimal**: Show only relevant code
- **Runnable**: Examples should compile and run
- **Clear**: Comments explain non-obvious parts
- **Realistic**: Use realistic data and scenarios

## Documentation Updates

### When to Update Documentation

- **API changes**: Update API documentation immediately
- **New features**: Document new functionality
- **Behavior changes**: Update relevant sections
- **Bug fixes**: Update if behavior changed significantly
- **Configuration changes**: Update configuration docs

### Documentation Checklist

Before committing:

- [ ] Public API documented with Doxygen comments
- [ ] README.md updated if needed
- [ ] Examples updated if API changed
- [ ] Design docs updated if architecture changed
- [ ] Third-person formal style used throughout
- [ ] File names follow lowercase_with_underscores convention
- [ ] All references to renamed files updated

## Inline Comments

### Code Comments

- **Why, not what**: Explain why code exists, not what it does
- **Complex logic**: Document non-obvious algorithms
- **TODOs**: Use `// TODO: description` for future work
- **FIXMEs**: Use `// FIXME: description` for known issues
- **Security notes**: Document security-sensitive code

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

1. **Third-person formal**: Never use first/second person in documentation
2. **File naming**: Use lowercase_with_underscores (e.g., `build.md`, `security.md`)
3. **README.md exception**: README.md stays uppercase (only exception to naming rule)
4. **Doxygen comments**: Document all public APIs with proper tags
5. **README structure**: Follow standard sections with badges and examples
6. **CONTRIBUTING**: Clear contribution guidelines
7. **Design docs**: Architecture and design decisions in `docs/`
8. **Compilable examples**: All examples must compile and run
9. **Update docs**: Keep documentation in sync with code changes
10. **Document why**: Inline comments should explain reasoning, not mechanics

## References

- See `QUICKSTART.md` → `quickstart.md` for quick start guide
- See `CONTRIBUTING.md` → `contributing.md` for contribution guidelines
- See `docs/architecture.md` for system architecture
- [Doxygen Documentation](https://www.doxygen.nl/manual/index.html)
