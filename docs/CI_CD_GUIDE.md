## CI/CD and Testing Guide (LLMEngine)

This document explains the Continuous Integration and Continuous Deployment (CI/CD) setup, testing strategy, and quality checks for the C++ `LLMEngine` project. It maps real repository assets (workflows, scripts, and tests) and provides concise commands to run the same checks locally.

## Table of Contents

- [Overview](#overview)
- [GitHub Actions Workflows](#github-actions-workflows)
  - [Core Testing Workflows](#core-testing-workflows)
  - [Quality Assurance Workflows](#quality-assurance-workflows)
  - [Performance and Compatibility Workflows](#performance-and-compatibility-workflows)
  - [Maintenance and Release Workflows](#maintenance-and-release-workflows)
- [Build Configurations](#build-configurations)
- [Testing Procedures](#testing-procedures)
  - [Unit and Integration Tests](#unit-and-integration-tests)
  - [Examples Build](#examples-build)
  - [Workflow Testing](#workflow-testing)
  - [Memory Safety Testing](#memory-safety-testing)
  - [Automated Testing in CI](#automated-testing-in-ci)
  - [Manual Testing](#manual-testing)
- [Quality Assurance](#quality-assurance)
- [Development Workflow](#development-workflow)
- [Local Development Setup](#local-development-setup)
- [Troubleshooting CI/CD Issues](#troubleshooting-cicd-issues)

## Overview

The LLMEngine project uses GitHub Actions for automated builds, tests, and quality checks across multiple configurations and environments. The pipeline runs on pushes and pull requests and includes:

- **Automated Builds**: CMake-based builds across Debug/Release and warning levels.
- **Extensive Testing**: Unit/integration tests under `test/`, and example builds under `examples/`.
- **Static Analysis**: Clang-Tidy, Actionlint, and CodeQL.
- **Memory Safety**: MemorySanitizer (Clang) in CI.
- **Performance & Compatibility**: Benchmarks and cross-distro builds.

[↑ Back to top](#cicd-and-testing-guide-llmengine)

## GitHub Actions Workflows

All workflows live in `.github/workflows/`. Each exists in this repository and is intended to be runnable as-is.

### Core Testing Workflows

- **Build and Test (`.github/workflows/test.yml`)**
  - Purpose: Compile the library and run core tests.
  - Typical checks: CMake configure, build, run tests from `test/`.

- **Comprehensive Test (`.github/workflows/comprehensive-test.yml`)**
  - Purpose: Exhaustive matrix of build options to prevent regressions.
  - Typical checks: multiple build types and warning levels; runs full tests when enabled.

- **Debug Build Test (`.github/workflows/debug-build-test.yml`)**
  - Purpose: Validate debug symbols, assertions, and basic debugger compatibility.

[↑ Back to top](#cicd-and-testing-guide-llmengine)

### Quality Assurance Workflows

- **Clang-Tidy (`.github/workflows/clang-tidy.yml`)**: Static analysis and style checks.
- **Memory Sanitizer (`.github/workflows/memory-sanitizer.yml`)**: MSan-based memory error detection (Clang required).
- **CodeQL (`.github/workflows/codeql.yml`)**: Security scanning of C/C++ code.
- **ShellCheck (`.github/workflows/shellcheck.yml`)**: Lint shell scripts in repo (`nextVersion/*.sh`, tests, helpers).
- **Actionlint (`.github/workflows/actionlint.yml`)**: Validate GitHub Actions YAML correctness.

[↑ Back to top](#cicd-and-testing-guide-llmengine)

### Performance and Compatibility Workflows

- **Performance Benchmark (`.github/workflows/performance-benchmark.yml`)**: Measure build/runtime performance over time.
- **Cross-Platform Test (`.github/workflows/cross-platform.yml`)**: Ensure builds on Ubuntu/Arch/Fedora/Debian.

[↑ Back to top](#cicd-and-testing-guide-llmengine)

### Maintenance and Release Workflows

- **Dependency Check (`.github/workflows/dependency-check.yml`)**: Scan system/library dependencies for issues.
- **Tag Cleanup (`.github/workflows/tag-cleanup.yml`)**: Maintain a clean tag namespace.
- **Version Bump (`.github/workflows/version-bump.yml`)**: Automate semantic versioning from Conventional Commits and tag creation.
- **Auto Release Notes (`.github/workflows/auto-release-notes.yml`)**: Generate release notes from commits/tags.
- **Dependabot Auto Merge (`.github/workflows/dependabot-auto-merge.yml`)**: Merge safe dependency PRs after checks.

[↑ Back to top](#cicd-and-testing-guide-llmengine)

## Build Configurations

The CI pipeline builds LLMEngine via CMake, testing multiple configurations to ensure robustness and performance. Prefer the same flags locally to reproduce CI.

### Basic Configurations

- **Default**: CMake default with `-O2`.
- **Performance**: `-O3 -march=native -mtune=native`, LTO enabled when available.
- **Debug**: `-g -O0`, assertions enabled.
- **Warnings**: `-Wall -Wextra -pedantic` for stricter checks.

### Combined Configurations

CI runs combinations such as:
- Performance + Warnings
- Debug + Warnings
- With/without tests enabled

### Configuration Details

- Performance: `-O3 -march=native -mtune=native`, optionally LTO.
- Debug: `-g -O0`, debug symbols verified; assertions enabled.
- Warnings: `-Wall -Wextra -pedantic`.

### Local Build Usage

Recommended local builds mirror CI using CMake. Use `make -j20` for compilation.

```sh
# From project root
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j20

# Debug build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j20

# Run CTest (if tests are enabled in the build)
ctest --output-on-failure
```

[↑ Back to top](#cicd-and-testing-guide-llmengine)

## Testing Procedures

The project uses layered tests: unit/integration tests under `test/`, example builds under `examples/`, and workflow checks via GitHub Actions. Temporary files must be written under `/tmp` only.

### Unit and Integration Tests

- Location: `test/`
- Entrypoints:
  - `test/build_test.sh` builds test targets with `make -j20`.
  - `test/run_api_tests.sh` runs API-focused tests (requires provider keys where applicable).

Example local run:

```sh
cd test
./build_test.sh
cd build_test
./test_llmengine
./test_api
```

### Examples Build

Example applications demonstrate usage and are also built in CI:

```sh
cd examples
./build_examples.sh
```

### Workflow Testing

Repository includes example workflow(s) under `test-workflows/` for CI experimentation only. Do not modify production workflows in `.github/workflows/` when testing—use `test-workflows/` exclusively for trial runs.

### Memory Safety Testing

- Memory Sanitizer (MSan) runs in CI via `.github/workflows/memory-sanitizer.yml` (Clang required).
- Local debug builds can be combined with sanitizers as needed.

### Automated Testing in CI

On pushes/PRs, GitHub Actions will:
1. Configure and build via CMake.
2. Validate binary characteristics (debug symbols, optimization flags).
3. Run tests from `test/` when enabled.
4. Execute static analysis and security checks.
5. Optionally run performance and cross-platform jobs.

### Manual Testing

```sh
# Debug build for troubleshooting
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j20
#gdb usage if an executable target exists
gdb build/bin/your-binary || true

# Verify optimizations (example if an executable binary exists)
objdump -d build/bin/your-binary | grep -i "-O3" || true

# Tests (from test/)
cd ../test
./build_test.sh
cd build_test
./test_llmengine
./test_api
```

[↑ Back to top](#cicd-and-testing-guide-llmengine)

## Quality Assurance

Quality Assurance (QA) is integrated throughout LLMEngine's lifecycle.

### Code Quality
- **Static Analysis**: Clang-Tidy and CodeQL.
- **Memory Safety**: MemorySanitizer in CI.
- **Security**: Dependency scanning of system and library dependencies.
- **Style Consistency**: ShellCheck and consistent CMake/C++ warnings.

### Performance
- **Optimization Verification**: Performance builds verify flags and LTO where applicable.
- **Benchmarking**: Automated benchmarks track trends.
- **Binary Size**: Monitored to avoid regressions.

### Compatibility
- **Cross-Platform**: CI validates Ubuntu, Arch, Fedora, and Debian builds.
- **Dependencies**: Keep system/library dependencies minimal and documented.
- **Portability**: Shell scripts linted with ShellCheck; avoid Bash dialect pitfalls.

### Reliability
- **Test Coverage**: Multiple targets and scenarios covered under `test/`.
- **Error Handling**: Tests assert robust and informative failures.
- **Edge Cases**: Include boundary conditions and negative paths.
- **Regression Testing**: CI prevents reintroducing fixed bugs.

[↑ Back to top](#cicd-and-testing-guide-llmengine)

## Development Workflow

This section outlines the typical development workflow for LLMEngine.

### Local Development
1. Setup: Install CMake, a recent GCC/Clang, and required libraries. See [README.md](../README.md) and [QUICKSTART.md](../QUICKSTART.md).
2. Build: Use CMake with `make -j20`.
3. Test: Use `test/build_test.sh`, or run built test targets directly.
4. Quality: Optionally run `clang-tidy` locally on `src/*.cpp`.
5. Commits: Follow Conventional Commits for automated versioning.

### CI/CD Integration
1. Push: Open PRs to trigger workflows.
2. Automated checks: Build, test, analysis, and security run automatically.
3. Review logs: Inspect failures in the Actions tab.
4. Fix and iterate: Push updates; CI re-runs.
5. Merge: After approvals and green checks.

### Release Process

1. Version bump: `.github/workflows/version-bump.yml` determines the semantic bump from Conventional Commits and updates tags.
2. Tagging: A new Git tag is created.
3. Validation: Comprehensive and QA workflows run on the tagged commit.
4. Release notes: Generated via `.github/workflows/auto-release-notes.yml`.

[↑ Back to top](#cicd-and-testing-guide-llmengine)

## Local Development Setup

Set up a local environment to build, test, and contribute effectively.

### Prerequisites

**System Requirements:**
- Linux distribution (Arch Linux, Ubuntu, Fedora, or Debian recommended)
- GCC or Clang compiler (version 10 or later)
- CMake (version 3.16 or later)
- Git
- Basic development tools (make, pkg-config, etc.)

**Arch Linux:**
```sh
sudo pacman -S base-devel cmake git
```

**Ubuntu/Debian:**
```sh
sudo apt update
sudo apt install build-essential cmake git
```

**Fedora:**
```sh
sudo dnf groupinstall "Development Tools"
sudo dnf install cmake git
```

### Repository Setup

```sh
# Clone the repository
git clone https://github.com/eserlxl/LLMEngine.git
cd LLMEngine

# Configure git user (if not already set)
git config user.name "Your Name"
git config user.email "your.email@example.com"
```

### Build Environment

```sh
# Default build
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j20

# Debug build for development
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j20
```

### Testing Setup

```sh
# Build tests and run
cd test
./build_test.sh
cd build_test
./test_llmengine
./test_api
```

**Workflow Tests:**
Use files under `test-workflows/` for local experimentation only.

**Memory Safety Testing:**
MSan runs in CI via `.github/workflows/memory-sanitizer.yml`.

### Development Tools

```sh
# Clang-Tidy (if available)
clang-tidy src/*.cpp -- -Isrc

# ShellCheck on scripts
shellcheck nextVersion/*.sh test/*.sh || true
```

**Debugging:**
```sh
# Build debug version
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j20

# Run with GDB (if an executable target exists)
gdb build/bin/your-binary || true

# Run with Valgrind (if available and applicable)
valgrind --leak-check=full ./test/build_test/test_llmengine
```

**Performance Analysis:**
```sh
# Profile with perf (if applicable)
perf record ./test/build_test/test_llmengine
perf report

# Check binary optimizations (if an executable target exists)
objdump -d build/bin/your-binary | grep -i "-O3" || true
```

[↑ Back to top](#cicd-and-testing-guide-llmengine)

## Troubleshooting CI/CD Issues

If you encounter issues with CI, use these steps to reproduce and diagnose locally.

### Common Issues
- **Build Failures**: Usually missing deps, compiler version mismatches, or syntax errors. Check logs.
- **Test Failures**: Reproduce locally using `test/build_test.sh` and run the failing binary.
- **Performance Issues**: Review hot paths and verify optimization flags.
- **Security Issues**: Update dependencies or fix insecure patterns reported by CodeQL.

### Debugging Workflows
- **Workflow Logs**: Inspect logs of the failing job in the Actions tab.
- **Local Reproduction**: Re-run the exact sequence locally using CMake and the test scripts.
- **Environment Differences**: Compare compiler and CMake versions.
- **Dependencies**: Ensure required system packages and libraries are installed.

### Local Debugging

**Build Issues:**
```sh
# Clean and rebuild (Debug)
rm -rf build
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j20

# Check toolchain
gcc --version
clang --version
cmake --version
```

**Test Issues:**
```sh
cd test
./build_test.sh
cd build_test
./test_llmengine
./test_api
```

**Memory Issues:**
```sh
# Build Debug and run under Valgrind (if applicable)
valgrind --leak-check=full ./test/build_test/test_llmengine
```

**Performance Issues:**
```sh
# Verify optimization flags on applicable binaries
objdump -d build/bin/your-binary | grep -i "-O3" || true

# Profile with perf (if applicable)
perf record ./test/build_test/test_llmengine
perf report
```

### Seeking Support
- **Docs**: See [README.md](../README.md), [QUICKSTART.md](../QUICKSTART.md), and [docs/CONFIGURATION.md](CONFIGURATION.md).
- **Issues**: Open issues in the repository with clear repro steps.
- **Contributing**: Follow [.github/CONTRIBUTING.md](../.github/CONTRIBUTING.md) when submitting PRs.

## See Also

- [docs/ARCHITECTURE.md](ARCHITECTURE.md) - System architecture
- [docs/API_REFERENCE.md](API_REFERENCE.md) - API documentation
- [docs/FAQ.md](FAQ.md) - Frequently asked questions
- [README.md](../README.md) - Main documentation