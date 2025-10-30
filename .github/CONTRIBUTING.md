## Contributing to LLMEngine

Thank you for your interest in contributing! We welcome bug reports, feature requests, and pull requests from everyone.

## How to Contribute

- **Bug Reports**: Use GitHub issues. Include steps to reproduce, environment details, and relevant logs/screenshots (redact secrets).
- **Feature Requests**: Open an issue describing your idea and why it helps. Check for existing issues first.
- **Pull Requests**: Fork and create a branch from `master`. Ensure your code follows project style and all tests pass. Include a clear description and reference related issues.

### Getting Started
- Build:
  - `cmake -S . -B build`
  - `cmake --build build --config Release -j20`
- Test:
  - `ctest --test-dir build --output-on-failure`
  - Or use scripts in `test/` (e.g., `./test/build_test.sh`, `./test/run_api_tests.sh`).

### Pull Request Checklist

Before submitting, please confirm:

1. Commits are clear; Conventional Commits style is encouraged (e.g., `feat: ...`, `fix: ...`).
2. Full test suite passes locally.
3. Code follows existing style and readability patterns.
4. Docs updated where appropriate (README/docs/examples).
5. No secrets are committed; use environment variables.

### Style
- C++17; match existing formatting and patterns.
- Prefer clarity and descriptive names over cleverness.

## Code of Conduct
By participating, you agree to abide by our [Code of Conduct](../CODE_OF_CONDUCT.md).

## Getting Help
Open an issue or start a discussion on GitHub if you have questions.