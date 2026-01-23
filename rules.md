# Cursor Rules for Modern C++ Code Generation

## Overview

This document serves as the index and guide for Cursor's code generation rules for LLMEngine. These rules ensure consistent, high-quality, production-ready C++23 code generation.

## How Cursor Should Apply These Rules

When generating or modifying C++ code, Cursor must:

1. **Read relevant rule files**: Consult the appropriate rule files based on the task
2. **Apply rules consistently**: Follow the guidelines in all generated code
3. **Create/modify CMake targets first**: Ensure build system is updated before code
4. **Add/extend tests**: Include or update tests with code changes
5. **Run static analysis**: Apply clang-tidy suggestions and fix low-risk items
6. **Ensure compilation**: Code must compile with warnings-as-errors
7. **Format code**: Apply clang-format automatically
8. **Provide rationale**: Document "why these choices" in change notes

## Rule Files Index

### Architecture and Structure

- **[00-architecture.md](.cursor/rules/00-architecture.md)**: Layering, module boundaries, ABI guidelines, pImpl usage, dependency rules
  - **When to use**: Designing new modules, defining public APIs, managing dependencies
  - **Key principles**: Public vs internal boundaries, ABI stability, dependency isolation

### Code Style

- **[10-cpp-style.md](.cursor/rules/10-cpp-style.md)**: Naming conventions, header/source structure, include hygiene, modern C++ features
  - **When to use**: Writing any C++ code, naming identifiers, organizing files
  - **Key principles**: PascalCase classes, snake_case functions, modern C++20/23 features

### Error Handling

- **[20-error-handling.md](.cursor/rules/20-error-handling.md)**: RAII-first approach, return-based error models, exception policy, logging
  - **When to use**: Implementing error handling, resource management, logging
  - **Key principles**: RAII, `std::expected`, exceptions sparingly, no exceptions across modules

### Performance

- **[30-performance.md](.cursor/rules/30-performance.md)**: Zero-cost abstractions, move semantics, container optimization, cache-friendly patterns
  - **When to use**: Optimizing code, designing data structures, performance-critical paths
  - **Key principles**: Move over copy, profile before optimize, avoid allocations in hot paths

### Threading and Concurrency

- **[40-threading.md](.cursor/rules/40-threading.md)**: Thread-safe patterns, `std::jthread`/`stop_token`, atomics vs mutex guidance
  - **When to use**: Writing concurrent code, thread-safe data structures, async operations
  - **Key principles**: Immutability by default, structured concurrency, no data races

### CMake Build System

- **[50-cmake.md](.cursor/rules/50-cmake.md)**: Modern CMake targets, compile commands export, warning flags, sanitizer presets
  - **When to use**: Setting up build configuration, adding new targets, managing dependencies
  - **Key principles**: Target-based CMake, modern patterns, proper dependency management

### Testing

- **[60-tests.md](.cursor/rules/60-tests.md)**: GoogleTest/Catch2 recommendation, test layout, coverage target, deterministic testing
  - **When to use**: Writing tests, setting up test infrastructure, ensuring test quality
  - **Key principles**: Mandatory tests for new code, deterministic tests, comprehensive coverage

### CI/CD

- **[70-ci.md](.cursor/rules/70-ci.md)**: GitHub Actions template, build configurations, test execution, coverage upload, clang-tidy job
  - **When to use**: Setting up CI, adding new build jobs, configuring workflows
  - **Key principles**: Multiple compiler/build matrix, sanitizers in Debug, coverage reporting

### Formatting and Static Analysis

- **[80-formatting.md](.cursor/rules/80-formatting.md)**: clang-format enforcement, clang-tidy checks, formatting verification in CI
  - **When to use**: Formatting code, running static analysis, configuring linting
  - **Key principles**: LLVM style, 120 cols, comprehensive clang-tidy checks

### Commit and Iteration

- **[90-commit-and-iteration.md](.cursor/rules/90-commit-and-iteration.md)**: Diff summary, Conventional Commits, atomic commits, task progression
  - **When to use**: Committing changes, organizing work, documenting decisions
  - **Key principles**: Atomic commits, Conventional Commits format, build/test before commit

### Documentation

- **[95-docs.md](.cursor/rules/95-docs.md)**: Doxygen-friendly comments, README structure, third-person formal style, compilable examples
  - **When to use**: Writing documentation, commenting code, creating examples
  - **Key principles**: Third-person formal style, Doxygen comments for public API, compilable examples

### Review Checklist

- **[99-review-checklist.md](.cursor/rules/99-review-checklist.md)**: Pre-commit gate checklist covering API clarity, ownership, error paths, UB prevention, documentation
  - **When to use**: Before committing, during code review, ensuring quality
  - **Key principles**: All checklist items must pass before commit

## Existing YAML Rules

The following YAML rules complement the markdown rules and provide enforcement triggers:

- **auto-commit-after-task.yaml**: Automatic commit after task completion
- **coding-standards.yaml**: Coding standards enforcement
- **cxx-strict-build-before-commit.yaml**: Strict build requirements before commit

These YAML rules remain active and work alongside the markdown rules.

## Code Generation Workflow

When Cursor generates C++ code, follow this workflow:

1. **Read relevant rules**: Identify which rule files apply to the task
2. **Create/modify CMake targets**: Update build system first
3. **Generate code**: Write code following style and architecture rules
4. **Add tests**: Create or update tests for new/changed code
5. **Run static analysis**: Apply clang-tidy and fix issues
6. **Format code**: Apply clang-format
7. **Verify compilation**: Build with warnings-as-errors
8. **Document rationale**: Add notes explaining design decisions
9. **Review checklist**: Verify all pre-commit items pass
10. **Commit**: Create atomic commit with Conventional Commits format

## Key Principles

### Modern C++

- **C++23 standard**: Use C++23 features
- **Modern C++**: Leverage latest C++23 capabilities
- **Zero-cost abstractions**: Prefer standard library abstractions
- **RAII**: Always use RAII for resource management

### Quality

- **Warnings-as-errors**: Code must compile cleanly
- **Tests mandatory**: New/changed code requires tests
- **Documentation**: Public API must be documented
- **Formatting**: Code must be formatted consistently

### Safety

- **No undefined behavior**: Code must be safe by construction
- **Error handling**: All error paths must be handled
- **Resource safety**: No leaks, use RAII
- **Thread safety**: No data races if multi-threaded

## Usage Examples

### Adding a New Feature

1. Read: `00-architecture.md`, `10-cpp-style.md`, `20-error-handling.md`
2. Create CMake target: `50-cmake.md`
3. Write code: Follow `10-cpp-style.md`
4. Add tests: `60-tests.md`
5. Document: `95-docs.md`
6. Review: `99-review-checklist.md`
7. Commit: `90-commit-and-iteration.md`

### Fixing a Bug

1. Read: `20-error-handling.md`, `60-tests.md`
2. Add regression test: `60-tests.md`
3. Fix code: Follow `10-cpp-style.md`
4. Verify: `99-review-checklist.md`
5. Commit: `90-commit-and-iteration.md`

### Refactoring

1. Read: `00-architecture.md`, `10-cpp-style.md`
2. Update CMake if needed: `50-cmake.md`
3. Update tests: `60-tests.md`
4. Update docs: `95-docs.md`
5. Review: `99-review-checklist.md`
6. Commit: `90-commit-and-iteration.md`

## References

- See individual rule files for detailed guidelines
- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/)
- [Modern CMake](https://cliutils.gitlab.io/modern-cmake/)
- [Conventional Commits](https://www.conventionalcommits.org/)

## Summary

These rules ensure that Cursor generates high-quality, modern C++ code that is:

- **Correct**: No undefined behavior, proper error handling
- **Maintainable**: Clear structure, good documentation, tests
- **Performant**: Efficient code, proper use of move semantics
- **Safe**: RAII, no resource leaks, thread-safe when needed
- **Consistent**: Following project conventions and style

Apply these rules consistently across all code generation tasks.

