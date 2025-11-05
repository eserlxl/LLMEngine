# Pre-Commit Review Checklist

## Overview

This document defines a pre-commit review checklist for LLMEngine. All code must pass this checklist before being committed. This ensures code quality, correctness, and maintainability.

## Pre-Commit Gate Checklist

### API Clarity

- [ ] **Public interfaces are clear and minimal**
  - Public API exposes only what is necessary
  - Function names clearly indicate purpose
  - Parameters are well-named and typed
  - Return types are explicit and documented

- [ ] **No accidental public exposure**
  - Internal implementation details not exposed in public headers
  - Third-party types not leaked in public API
  - Private members properly encapsulated

### Ownership and Lifetimes

- [ ] **Explicit ownership semantics**
  - Smart pointers used correctly (`std::unique_ptr`, `std::shared_ptr`)
  - No raw `new`/`delete` (except in rare, justified cases)
  - RAII used for all resources
  - Move semantics used appropriately

- [ ] **Lifetime management is clear**
  - Objects don't outlive their dependencies
  - No dangling references or pointers
  - Resource cleanup is automatic (RAII)
  - No resource leaks

### Error Paths Tested

- [ ] **All error paths have tests**
  - Expected failures tested (network errors, invalid input, etc.)
  - Error handling code is exercised
  - Edge cases and boundary conditions tested
  - Failure modes are covered

- [ ] **Error handling is robust**
  - Errors are handled gracefully
  - No resource leaks on error paths
  - Error messages are clear and actionable
  - Error propagation is correct

### Undefined Behavior Prevention

- [ ] **No undefined behavior possible**
  - No uninitialized variables
  - No out-of-bounds access
  - No null pointer dereferences
  - No use-after-move (unless intentional)
  - No data races (if multi-threaded)
  - No integer overflow (if relevant)

- [ ] **Bounds checking**
  - Array/vector access is bounds-checked or guaranteed safe
  - String operations are safe
  - Iterator usage is valid

- [ ] **Type safety**
  - No unsafe casts (prefer `static_cast` over C-style casts)
  - No `reinterpret_cast` without justification
  - Strong types used where appropriate (e.g., `enum class`)

### Compilation

- [ ] **Code compiles with warnings-as-errors**
  - Builds cleanly with `-Werror` enabled
  - No compiler warnings
  - No deprecation warnings
  - Compiles in both Debug and Release modes

- [ ] **CMake configuration is correct**
  - Modern CMake patterns used (target-based)
  - Dependencies properly declared
  - Installation rules correct (if applicable)

### Formatting

- [ ] **Code is formatted**
  - `clang-format` has been applied
  - Formatting matches project style
  - No formatting inconsistencies

- [ ] **No clang-tidy warnings**
  - Static analysis passes (if enabled)
  - No high-priority issues
  - Suppressions documented if needed

### Documentation

- [ ] **Public API is documented**
  - Doxygen comments for all public functions/classes
  - Parameters and return values documented
  - Examples provided for complex APIs
  - Notes and warnings included where relevant

- [ ] **Documentation is updated**
  - README updated if user-facing changes
  - API reference updated if API changed
  - Design docs updated if architecture changed
  - Examples updated if usage changed

- [ ] **Third-person formal style**
  - Documentation uses third person
  - No first/second person ("I", "we", "you")
  - Professional, clear tone

### Testing

- [ ] **Tests are included**
  - New code has corresponding tests
  - Changed code has updated tests
  - Tests cover normal and error paths
  - Tests are deterministic (no sleeps, no network)

- [ ] **Tests pass**
  - All tests pass locally
  - No flaky tests
  - Tests run in reasonable time
  - Integration tests pass (if applicable)

### Performance

- [ ] **Performance considerations**
  - No obvious performance issues introduced
  - Hot paths are efficient
  - No unnecessary allocations
  - Move semantics used where appropriate

### Security

- [ ] **No security issues**
  - Input validation present
  - No buffer overflows possible
  - No unsafe string operations
  - Sensitive data handled appropriately

## Automated Checks

The following checks should be automated in CI:

1. **Build**: Compiles with `-Werror`
2. **Tests**: All tests pass
3. **Formatting**: Code is formatted
4. **clang-tidy**: Static analysis passes
5. **Coverage**: Minimum coverage maintained (if applicable)

## Manual Review

Before committing, manually verify:

1. **Code review**: Read through the changes
2. **Logic correctness**: Verify algorithm/logic is correct
3. **Edge cases**: Consider edge cases and error conditions
4. **Documentation**: Check that documentation is clear
5. **Test coverage**: Verify tests adequately cover changes

## Checklist Usage

### Before Every Commit

1. Run through the checklist
2. Verify all items are checked
3. Fix any issues found
4. Commit only when all items pass

### In Code Review

- Reviewers should verify checklist items
- Point out any missed items
- Require fixes before approval

## Summary

This checklist ensures:

1. **API clarity**: Public interfaces are clear and minimal
2. **Ownership**: Explicit, RAII-managed lifetimes
3. **Error paths**: All error paths tested
4. **UB prevention**: No undefined behavior possible
5. **Compilation**: Builds cleanly with warnings-as-errors
6. **Formatting**: Code is formatted and linted
7. **Documentation**: Public API documented and updated
8. **Testing**: Tests included and passing
9. **Performance**: No obvious performance issues
10. **Security**: No security vulnerabilities

## References

- See `00-architecture.md` for API design
- See `10-cpp-style.md` for code style
- See `20-error-handling.md` for error handling
- See `60-tests.md` for testing requirements
- See `80-formatting.md` for formatting rules
- See `90-commit-and-iteration.md` for commit workflow

