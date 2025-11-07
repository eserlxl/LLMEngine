# Code Formatting Rules

## Overview

This document defines code formatting and static analysis rules for LLMEngine. These rules ensure consistent code style and catch potential issues early.

## clang-format

### Configuration

- **Style**: LLVM baseline with project-specific tweaks
- **File**: `.clang-format` in project root
- **Column limit**: 100 characters
- **Indentation**: 4 spaces (no tabs)

### Current Configuration

The project uses the following `.clang-format` settings:

```
BasedOnStyle: LLVM
Language: Cpp
IndentWidth: 4
TabWidth: 4
UseTab: Never
ColumnLimit: 100
AccessModifierOffset: -2
BreakBeforeBinaryOperators: NonAssignment
AlwaysBreakTemplateDeclarations: MultiLine
BinPackArguments: false
BinPackParameters: false
IncludeBlocks: Regroup
DerivePointerAlignment: false
PointerAlignment: Left
ReflowComments: true
SortIncludes: true
SpaceBeforeParens: ControlStatements
AllowShortFunctionsOnASingleLine: Empty
SpacesInContainerLiterals: false
```

### Formatting Enforcement

Formatting enforcement is configured in `formatting-enforcement.yaml`. See that file for detailed configuration.

#### In IDE (Cursor)

- **Format on save**: Automatic formatting on save (enforced via formatting-enforcement.yaml)
- **Format on paste**: Automatic formatting on paste
- **Format selection**: Use `Shift+Alt+F` (or equivalent) to format selection

#### In CI

- **Format check job**: Verify formatting in CI (see `70-ci.md`)
- **Fail on format errors**: Reject commits with unformatted code

```yaml
- name: Check formatting
  run: |
    find src include test -name "*.cpp" -o -name "*.hpp" | \
      xargs clang-format --dry-run --Werror
```

### Manual Formatting

CI installs and uses `clang-format-20` (the maximum available version on GitHub runners). For local development, use the highest available version (preferably 20 or 21) to ensure parity with CI.

**Recommended**: Install `clang-format-20` or `clang-format-21` locally:
```bash
# On Ubuntu/Debian
sudo apt-get install -y clang-format-20
# Or if version 21 is available in your distribution
sudo apt-get install -y clang-format-21
```

Format all files:

```bash
find src include test -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i
```

Format single file:

```bash
clang-format -i path/to/file.cpp
```

**Note**: If you have a versioned binary installed (e.g., `clang-format-20` or `clang-format-21`), you can use it directly or create a symlink to ensure the `clang-format` command uses the correct version.

## clang-tidy

### Configuration

- **File**: `.clang-tidy` in project root
- **Checks**: Comprehensive set of checks enabled
- **Header filter**: Only check project code, not dependencies

### Current Configuration

The project uses the following `.clang-tidy` settings:

```
Checks: |
  -*,
  bugprone-*,
  -bugprone-easily-swappable-parameters,
  clang-analyzer-*,
  performance-*,
  -performance-avoid-endl,
  modernize-*,
  -modernize-pass-by-value,
  -modernize-use-trailing-return-type,
  readability-*,
  -readability-identifier-length,
  -readability-function-cognitive-complexity,
  -readability-qualified-auto,
  -readability-braces-around-statements,
  -readability-simplify-boolean-expr,
  -readability-implicit-bool-conversion,
  -readability-identifier-naming,
  portability-*,
  hicpp-use-auto
WarningsAsErrors: ''
HeaderFilterRegex: '^(src|examples|test)/.*'
FormatStyle: none
```

### Enabled Checks

- **bugprone-***: Bug-prone patterns
- **clang-analyzer-***: Static analysis
- **performance-***: Performance issues
- **modernize-***: Modern C++ suggestions
- **readability-***: Readability improvements
- **portability-***: Portability issues
- **hicpp-use-auto**: High Integrity C++ suggestions

### Suppressions Policy

#### Inline Suppressions

Suppress specific warnings with comments:

```cpp
// NOLINTNEXTLINE(bugprone-use-after-move)
auto moved = std::move(value);
```

#### File-Level Suppressions

Suppress warnings for entire files in `.clang-tidy`:

```
# Suppress readability-identifier-naming for generated files
# NOLINTBEGIN(readability-identifier-naming)
// Generated code
# NOLINTEND
```

#### Configuration Suppressions

Add suppressions to `.clang-tidy`:

```yaml
CheckOptions:
  - key: readability-identifier-naming.ClassCase
    value: CamelCase
```

### Running clang-tidy

#### Via CMake

Enable clang-tidy in CMake:

```cmake
option(ENABLE_CLANG_TIDY "Run clang-tidy if available" OFF)

if(ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY_EXE NAMES clang-tidy)
  if(CLANG_TIDY_EXE)
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE};-warnings-as-errors=*;-header-filter=^(src|examples|test)/.*")
  endif()
endif()
```

Build with clang-tidy:

```bash
cmake -S . -B build -DENABLE_CLANG_TIDY=ON
cmake --build build
```

#### Manual Execution

Run clang-tidy on specific files:

```bash
clang-tidy -p build/compile_commands.json src/MyFile.cpp
```

Run on all files:

```bash
find src include test -name "*.cpp" -o -name "*.hpp" | \
  xargs clang-tidy -p build/compile_commands.json
```

## Formatting Best Practices

### Include Order

Follow the include order defined in `10-cpp-style.md`:

1. Corresponding header
2. System headers
3. Third-party headers
4. Project headers

clang-format will sort includes within each block.

### Pointer/Reference Alignment

- **Left alignment**: `int* ptr` (not `int *ptr`)
- **References**: `int& ref` (not `int &ref`)

### Spacing

- **Control statements**: Space before parentheses: `if (condition)`
- **Functions**: No space: `void function()`
- **Operators**: Consistent spacing around operators

### Line Length

- **Limit**: 100 characters
- **Break long lines**: Break at logical points (operators, commas)
- **Indentation**: Proper indentation for continuation lines (4 spaces)

## Static Analysis Integration

### Pre-commit Hooks

Consider adding pre-commit hooks for formatting:

```bash
#!/bin/sh
# .git/hooks/pre-commit

# Format staged files
git diff --cached --name-only --diff-filter=ACM | \
  grep -E '\.(cpp|hpp)$' | \
  xargs clang-format -i

# Re-stage formatted files
git add -u
```

### CI Integration

- **Format check**: Verify formatting in CI (see `70-ci.md`)
- **clang-tidy**: Run static analysis in CI
- **Fail on errors**: Reject commits with formatting or analysis errors

## Summary

1. **clang-format**: LLVM style, 100 cols, 4-space indent
2. **Format on save**: Automatic in IDE (configured via formatting-enforcement.yaml)
3. **Pre-commit hooks**: Automatic formatting on commit
4. **Format check**: Verify in CI
5. **clang-tidy**: Comprehensive checks enabled
6. **Suppressions**: Document and justify suppressions
7. **CI integration**: Format and analysis checks in CI

## Configuration Files

- **`.clang-format`**: Main formatting configuration file
- **`formatting-enforcement.yaml`**: Enforcement rules and settings
- **`.pre-commit-config.yaml`**: Pre-commit hook configuration
- **`.github/workflows/ci.yml`**: CI format checking

## References

- See `10-cpp-style.md` for code style details
- See `70-ci.md` for CI integration
- [clang-format Documentation](https://clang.llvm.org/docs/ClangFormat.html)
- [clang-tidy Documentation](https://clang.llvm.org/extra/clang-tidy/)

