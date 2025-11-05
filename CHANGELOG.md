# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Comprehensive Cursor rules system for modern C++ code generation
  - Architecture rules (`00-architecture.md`): Layering, module boundaries, ABI guidelines, pImpl usage, dependency rules
  - C++ style guide (`10-cpp-style.md`): Naming conventions, header/source structure, include hygiene, modern C++ features
  - Error handling rules (`20-error-handling.md`): RAII-first approach, return-based error models, exception policy, logging
  - Performance guidelines (`30-performance.md`): Zero-cost abstractions, move semantics, container optimization, cache-friendly patterns
  - Threading rules (`40-threading.md`): Thread-safe patterns, `std::jthread`/`stop_token`, atomics vs mutex guidance
  - CMake rules (`50-cmake.md`): Modern CMake targets, compile commands export, warning flags, sanitizer presets
  - Testing rules (`60-tests.md`): GoogleTest/Catch2 recommendation, test layout, coverage target, deterministic testing
  - CI/CD rules (`70-ci.md`): GitHub Actions template, build configurations, test execution, coverage upload, clang-tidy job
  - Formatting rules (`80-formatting.md`): clang-format enforcement, clang-tidy checks, formatting verification in CI
  - Commit rules (`90-commit-and-iteration.md`): Diff summary, Conventional Commits, atomic commits, task progression
  - Documentation rules (`95-docs.md`): Doxygen-friendly comments, README structure, third-person formal style, compilable examples
  - Review checklist (`99-review-checklist.md`): Pre-commit gate checklist covering API clarity, ownership, error paths, UB prevention
- Top-level `RULES.md` index that explains how Cursor should apply these rules
- Change notes template (`.cursor/last-change-notes.md`) for documenting rationale and design decisions

### Changed

- Enhanced code generation workflow to follow comprehensive rule system
- Migration from YAML-focused rules to markdown-based documentation with YAML enforcement triggers
- Existing YAML rules (auto-commit, coding-standards, strict-build) remain active and complement markdown rules

### Notes

- All rule files are located in `.cursor/rules/` directory
- Markdown rules serve as primary documentation and generation guidance
- YAML rules continue to provide enforcement triggers for Cursor
- Rules align with existing project structure and conventions (C++20, CMake 3.20+, modern practices)

