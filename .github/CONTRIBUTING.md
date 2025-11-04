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

### Typical workflow

```bash
git checkout -b feature/name
cd test
./run_api_tests.sh
git add -A
git commit -m "feat: add feature/name"
git push origin feature/name
```

### Pull Request Checklist

Before submitting, please confirm:

1. Commits are clear; Conventional Commits style is encouraged (e.g., `feat: ...`, `fix: ...`).
2. Full test suite passes locally.
3. Code follows existing style and readability patterns.
4. Docs updated where appropriate (README/docs/examples).
5. No secrets are committed; use environment variables.

### Style
- C++20; match existing formatting and patterns.
- Prefer clarity and descriptive names over cleverness.

## Code of Conduct
By participating, you agree to abide by our [Code of Conduct](../CODE_OF_CONDUCT.md).

## Getting Help
Open an issue or start a discussion on GitHub if you have questions.

## See Also

- [README.md](../README.md) - Main documentation
- [QUICKSTART.md](../QUICKSTART.md) - Quick start guide
- [docs/CI_CD_GUIDE.md](../docs/CI_CD_GUIDE.md) - CI/CD and testing guide
- [docs/FAQ.md](../docs/FAQ.md) - Frequently asked questions
- [docs/API_REFERENCE.md](../docs/API_REFERENCE.md) - API documentation