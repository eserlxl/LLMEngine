# Contributing to LLMEngine

Thank you for your interest in contributing!

## Build

- Configure with CMake presets:
  - Debug: `cmake --preset debug`
  - RelWithDebInfo: `cmake --preset relwithdebinfo`
  - Release: `cmake --preset release`
- Build with make: `cmake --build build -j20`

## Tests

- Build tests with the presets above (BUILD_TESTING=ON by default).
- Run tests: `ctest --preset ctest-debug` or from build dir: `ctest -V`.

## Coding style

- C++23, target-based CMake, no global flags.
- Prefer RAII, explicit ownership, `enum class`, and `[[nodiscard]]` where relevant.
- Avoid naked new/delete; use smart pointers.

## Security & privacy

- Never log credentials. Use `RequestLogger` for redacted logs.
- Prefer env vars over files for API keys.

## License

- This project is licensed under GPLv3. By contributing, it is agreed contributions will be under GPLv3.


