# Build Guide

This document provides comprehensive information about building LLMEngine, including CMake presets and build options.

## 🧰 Build Presets

The repo includes a `CMakePresets.json` file with ready-to-use presets:

```bash
cmake --preset debug
cmake --preset relwithdebinfo
cmake --preset release
```

Build with make:

```bash
cmake --build build -j$(nproc)
```

Optional build toggles:

```bash
cmake --preset debug -DLLM_ENABLE_ASAN=ON -DLLM_ENABLE_UBSAN=ON
cmake --preset release -DLLM_ENABLE_LTO=ON
cmake --preset debug -DENABLE_COVERAGE=ON
```

A consumer example using `find_package(LLMEngine)` exists in `examples/consumer`.

---

## 🔧 Build Options

LLMEngine supports various CMake options to customize the build configuration. These options control compiler flags, optimization levels, sanitizers, and other build-time features.

### Core Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `DEBUG_MODE` | `OFF` | Enable debug build flags (`-O0 -g`, `DEBUG` macro). Mutually exclusive with `PERFORMANCE_BUILD`. |
| `PERFORMANCE_BUILD` | `OFF` | Enable performance optimizations (`-O3`, `NDEBUG`). Mutually exclusive with `DEBUG_MODE`. |
| `WARNING_MODE` | `ON` | Enable extra compiler warnings (`-Wall -Wextra -Wpedantic`, etc.). |
| `BUILD_TESTING` | `ON` | Build tests and enable CTest. |
| `ENABLE_EXAMPLES` | `ON` | Build example programs. |
| `BUILD_SHARED_LIBS` | `OFF` | Build shared libraries instead of static. |
| `ENABLE_DOXYGEN` | `ON` | Enable Doxygen documentation target. |

### Performance Options

| Option | Default | Description |
|--------|---------|-------------|
| `ENABLE_NATIVE_OPTIMIZATION` | `OFF` | Use `-march=native -mtune=native` in performance builds (CPU-specific optimizations). |
| `LLM_ENABLE_LTO` | `OFF` | Force enable IPO/LTO (Inter-Procedural Optimization / Link-Time Optimization) when supported. |

### Sanitizer Options

These options enable various sanitizers for debugging (only effective when `DEBUG_MODE=ON`):

| Option | Default | Description |
|--------|---------|-------------|
| `ENABLE_SANITIZERS` | `OFF` | Enable AddressSanitizer and UndefinedBehaviorSanitizer. |
| `LLM_ENABLE_ASAN` | `OFF` | Enable AddressSanitizer (overrides `ENABLE_SANITIZERS`). |
| `LLM_ENABLE_UBSAN` | `OFF` | Enable UndefinedBehaviorSanitizer (overrides `ENABLE_SANITIZERS`). |
| `LLM_ENABLE_MSAN` | `OFF` | Enable MemorySanitizer (Clang-only). |
| `LLM_ENABLE_TSAN` | `OFF` | Enable ThreadSanitizer. |

### Dependency Options

| Option | Default | Description |
|--------|---------|-------------|
| `LLM_DISABLE_FETCHCONTENT` | `OFF` | Disable FetchContent; require system packages (fail if dependencies not found). |

### Output Options

| Option | Default | Description |
|--------|---------|-------------|
| `LLM_VERBOSE_CONFIG_SUMMARY` | `ON` | Print detailed build configuration summary. Set to `OFF` for CI-friendly concise output. |

### Example Build Configurations

**Debug build with sanitizers:**
```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DDEBUG_MODE=ON \
  -DENABLE_SANITIZERS=ON
```

**Performance build with native optimizations:**
```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DPERFORMANCE_BUILD=ON \
  -DENABLE_NATIVE_OPTIMIZATION=ON \
  -DLLM_ENABLE_LTO=ON
```

**CI-friendly build (minimal output):**
```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLM_VERBOSE_CONFIG_SUMMARY=OFF \
  -DBUILD_TESTING=ON
```

**Build with system dependencies (no FetchContent):**
```bash
cmake -S . -B build \
  -DLLM_DISABLE_FETCHCONTENT=ON
```

### Build Presets

The project includes CMake presets for common configurations:

- `debug`: Debug build with sanitizers enabled
- `release`: Release build with performance optimizations
- `relwithdebinfo`: Release build with debug symbols

Use presets with:
```bash
cmake --preset debug
cmake --build build
```

For more details on build options and their interactions, see [CMakeLists.txt](../CMakeLists.txt) and [docs/CI_CD_GUIDE.md](CI_CD_GUIDE.md).

