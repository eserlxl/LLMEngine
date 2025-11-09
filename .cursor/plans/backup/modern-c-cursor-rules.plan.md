<!-- aada48c9-24f3-4ff8-8071-97d885c92184 b4a13245-6173-48d1-ac3f-d065786b166e -->
# Modern C++ Cursor Rules Implementation Plan

## Overview

Transform the existing YAML-based Cursor rules into a comprehensive markdown-based rule system for modern C++ code generation. The rules will guide Cursor to consistently generate high-quality, production-ready C++20/23 code with proper architecture, testing, and CI integration.

## Current State Analysis

- **Existing rules**: 3 YAML files in `.cursor/rules/` (auto-commit, coding-standards, strict-build)
- **Formatting**: `.clang-format` (LLVM style, 120 cols) and `.clang-tidy` (comprehensive checks) present
- **CMake**: Modern CMake 3.20+ with target-based structure, comprehensive options
- **CI**: Extensive GitHub Actions workflows (14+ workflow files)
- **Tests**: Custom test executables (no formal framework detected)
- **Documentation**: Well-structured `docs/` directory with multiple markdown files

## Implementation Tasks

### 1. Create Core Rule Files (`.cursor/rules/*.md`)

#### 1.1 `00-architecture.md`

- Define layering: public headers (`include/LLMEngine/`), internal implementation (`src/`), tests (`test/`)
- Module boundaries: public API vs internal details
- ABI stability guidelines for shared libraries
- pImpl pattern usage when appropriate
- Dependency rules: stdlib-first, third-party isolation behind interfaces
- Header hygiene: forward declarations, minimal includes

#### 1.2 `10-cpp-style.md`

- Naming: PascalCase classes, snake_case functions/variables, UPPER_CASE constants
- Header/source structure: one class per file, matching names
- Include order: corresponding header, system headers, third-party, project headers
- C++ Core Guidelines alignment
- Modern C++ features: `constexpr`, `const`, `noexcept`, `[[nodiscard]]`
- Type safety: `std::span`, ranges, concepts, `enum class`
- RAII, zero raw `new`/`delete`, smart pointers
- Strong typedefs for type safety
- Filesystem/time APIs (`std::filesystem`, `std::chrono`)

#### 1.3 `20-error-handling.md`

- RAII-first approach
- Return-based error models: prefer `std::expected` (C++23) or status objects
- Exceptions: only for exceptional flows, never across module boundaries
- Resource safety: never leak resources
- Logging: lightweight logger, no global singletons
- Error propagation patterns

#### 1.4 `30-performance.md`

- Zero-cost abstractions
- Move semantics: prefer moves, avoid unnecessary copies
- Container optimization: `reserve()`, small-buffer optimizations
- Cache friendliness
- Avoid allocations in hot paths
- Profile-before-optimize principle
- Benchmark-guided decisions

#### 1.5 `40-threading.md`

- Thread-safe patterns
- Immutability by default
- `std::jthread`/`stop_token` for cancellable tasks
- Atomics vs mutex guidance
- No data races: structured concurrency
- Avoid global mutable state
- Thread-local storage when appropriate

#### 1.6 `50-cmake.md`

- Modern CMake targets: `target_sources`, `target_link_libraries`, `target_compile_features`
- Position-independent code: `CMAKE_POSITION_INDEPENDENT_CODE ON`
- Export compile commands: `CMAKE_EXPORT_COMPILE_COMMANDS ON`
- Warning flags: `-Wall -Wextra -Wpedantic -Wconversion -Wshadow` per-target
- Sanitizer presets: ASan/UBSan/LSan in Debug mode (behind toggles)
- Minimum CMake policy usage
- CMake 3.20+ required

#### 1.7 `60-tests.md`

- Framework: Recommend GoogleTest (preferred) or Catch2 for new tests
- Test layout: `test/` directory, mirror source structure
- Coverage target: `ENABLE_COVERAGE` option
- Rule: new/changed code ⇒ new/updated tests
- Deterministic tests: no sleeps, no network unless mocked
- Test naming: descriptive names, test fixtures when appropriate
- Integration vs unit tests separation

#### 1.8 `70-ci.md`

- GitHub Actions template: `ci.yml` as reference
- Configure step: CMake with proper options
- Build: Debug + Release configurations
- Run tests with sanitizers in Debug builds
- Upload coverage artifacts
- clang-tidy job: separate lint job
- Cache dependencies: CMake cache, dependency downloads

#### 1.9 `80-formatting.md`

- Enforce `.clang-format` (LLVM baseline with project tweaks)
- Run formatter on save in Cursor
- `clang-tidy` default checks (already configured)
- Suppressions policy: document in `.clang-tidy` comments
- Format verification in CI

#### 1.10 `90-commit-and-iteration.md`

- After each completed task: show diff summary and checklist
- Commit with Conventional Commits style
- Ask to proceed to next task (unless auto-commit rule active)
- Never batch unrelated changes
- Atomic, reversible commits

#### 1.11 `95-docs.md`

- Doxygen-friendly comments: `/** */` for public APIs
- README structure: overview, quick start, API reference links
- CONTRIBUTING guidelines
- Design docs: live in `docs/` directory
- Third-person formal style: never first/second person
- Minimal compilable code samples

#### 1.12 `99-review-checklist.md`

- Pre-commit gate checklist:
  - API clarity: public interfaces are clear and minimal
  - Ownership and lifetimes: explicit, RAII-managed
  - Error paths tested: failure modes covered
  - UB impossible: no undefined behavior by construction
  - Compiles with warnings-as-errors
  - Formatting clean: clang-format applied
  - Docs updated: public API changes documented

### 2. Create Top-Level Index (`RULES.md`)

- Index all rule files with brief descriptions
- Explain how Cursor should apply rules during generation/edits
- Reference existing YAML rules where they complement markdown rules
- Usage guide for developers

### 3. Update CI Workflow (`.github/workflows/cpp.yml` or enhance existing)

- Ensure `ci.yml` follows best practices from `70-ci.md`
- Add coverage upload step if missing
- Verify clang-tidy job exists and is properly configured
- Add format check job if missing

### 4. Verify/Update Formatting Configs

- Review `.clang-format`: ensure LLVM baseline with project tweaks (120 cols, 2-space indent)
- Review `.clang-tidy`: ensure comprehensive checks are enabled
- Add suppression comments policy if needed

### 5. Create Change Notes Template (`.cursor/last-change-notes.md`)

- Template for storing rationale for code generation decisions
- Include "why these choices" explanations
- Reference in `90-commit-and-iteration.md`

### 6. Create CHANGELOG Entry

- Document new rules system
- Explain migration from YAML-focused to markdown-focused
- Note backward compatibility with existing YAML rules

## File Structure

```
.cursor/
  rules/
    00-architecture.md          (new)
    10-cpp-style.md             (new)
    20-error-handling.md        (new)
    30-performance.md           (new)
    40-threading.md             (new)
    50-cmake.md                 (new)
    60-tests.md                 (new)
    70-ci.md                    (new)
    80-formatting.md            (new)
    90-commit-and-iteration.md  (new)
    95-docs.md                  (new)
    99-review-checklist.md      (new)
    auto-commit-after-task.yaml (existing, keep)
    coding-standards.yaml       (existing, keep)
    cxx-strict-build-before-commit.yaml (existing, keep)
  last-change-notes.md          (new template)
RULES.md                        (new index)
CHANGELOG.md                    (update or create)
```

## Key Decisions

1. **Markdown over YAML**: Markdown provides better readability and documentation
2. **Keep YAML rules**: Existing YAML rules remain for Cursor's rule matching system
3. **Test framework**: Recommend GoogleTest/Catch2 for new tests, but support existing custom tests
4. **C++ standard**: C++20 minimum, C++23 features when safe
5. **CMake minimum**: 3.20 (already enforced)

## Deliverables Checklist

- [ ] 12 markdown rule files created in `.cursor/rules/`
- [ ] `RULES.md` top-level index created
- [ ] CI workflow verified/updated (`.github/workflows/ci.yml`)
- [ ] `.clang-format` and `.clang-tidy` verified
- [ ] `.cursor/last-change-notes.md` template created
- [ ] CHANGELOG entry added
- [ ] Final commit with message: `chore(rules): establish modern C++ generation & review rules (style, tests, CI, docs)`

## Notes

- Existing YAML rules will be preserved and referenced in markdown rules
- Markdown rules serve as primary documentation and generation guidance
- YAML rules continue to provide enforcement triggers for Cursor
- All rules align with existing project structure and conventions

### To-dos

- [ ] Create 00-architecture.md with layering, module boundaries, ABI guidelines, pImpl usage, and dependency rules
- [ ] Create 10-cpp-style.md with naming conventions, header/source structure, include hygiene, modern C++ features, and Core Guidelines alignment
- [ ] Create 20-error-handling.md with RAII-first approach, return-based error models, exception policy, and logging guidelines
- [ ] Create 30-performance.md with zero-cost abstractions, move semantics, container optimization, and cache-friendly patterns
- [ ] Create 40-threading.md with thread-safe patterns, std::jthread/stop_token, atomics vs mutex guidance, and structured concurrency
- [ ] Create 50-cmake.md with modern CMake targets, compile commands export, warning flags, and sanitizer presets
- [ ] Create 60-tests.md with GoogleTest/Catch2 recommendation, test layout, coverage target, and deterministic testing requirements
- [ ] Create 70-ci.md with GitHub Actions template, build configurations, test execution, coverage upload, and clang-tidy job
- [ ] Create 80-formatting.md with clang-format enforcement, clang-tidy checks, and formatting verification in CI
- [ ] Create 90-commit-and-iteration.md with diff summary, Conventional Commits, atomic commits, and task progression
- [ ] Create 95-docs.md with Doxygen-friendly comments, README structure, third-person formal style, and compilable code samples
- [ ] Create 99-review-checklist.md with pre-commit gate checklist covering API clarity, ownership, error paths, UB prevention, and documentation
- [ ] Create RULES.md top-level index that references all rule files and explains how Cursor should apply them
- [ ] Verify .github/workflows/ci.yml follows best practices, add coverage upload if missing, ensure clang-tidy job exists
- [ ] Review and verify .clang-format and .clang-tidy are properly configured and aligned with project standards
- [ ] Create .cursor/last-change-notes.md template for storing rationale and generation decisions
- [ ] Add CHANGELOG entry documenting the new rules system and migration notes
- [ ] Commit all changes with message: 'chore(rules): establish modern C++ generation & review rules (style, tests, CI, docs)'