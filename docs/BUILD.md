# Build Guide

This document provides comprehensive information about building LLMEngine, including dependencies, CMake presets, and build options.

## 📦 Install Dependencies

Before building, install the required dependencies for your platform:

**Ubuntu/Debian**
```bash
sudo apt install build-essential cmake libssl-dev nlohmann-json3-dev libcpr-dev
```

**Arch Linux**
```bash
sudo pacman -S base-devel cmake openssl nlohmann-json cpr
```

**macOS (Homebrew)**
```bash
brew install cmake openssl nlohmann-json cpr
```

---

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

Examples and tests prefer consuming the exported package:

```cmake
find_package(LLMEngine CONFIG REQUIRED)
target_link_libraries(your_example PRIVATE llmengine::LLMEngine llmengine::compile_options)
```

In build-tree scenarios, the examples will also attempt `find_package(LLMEngine CONFIG QUIET)` after prefixing `${PROJECT_SOURCE_DIR}/build` to `CMAKE_PREFIX_PATH`. Direct references to `../build/libLLMEngine.a` are guarded and no longer required for normal workflows.

---

## 🛠️ Build Script

The repository includes a convenient `build.sh` script that simplifies the build process:

```bash
./build.sh
```

The `build.sh` script supports the following options:

- **performance**: Enable performance optimizations (mutually exclusive with debug)
- **warnings**: Enable extra compiler warnings
- **debug**: Enable debug mode (mutually exclusive with performance)
- **clean**: Remove build directory and reconfigure
- **tests**: Build and run tests (uses ctest if available)
- **-j, --jobs N**: Parallel build jobs (default: auto-detected)
- **--build-dir DIR**: Custom build directory (default: `build`)

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

---

## 🧪 Build and Test

Build the project using CMake:

```bash
cmake -S . -B build
cmake --build build --config Release -j$(nproc)
ctest --test-dir build --output-on-failure
```

### Run Examples

Build and run example programs:

```bash
bash examples/build_examples.sh
./examples/build_examples/chatbot
```

---

For more details on build options and their interactions, see [CMakeLists.txt](../CMakeLists.txt) and [docs/CI_CD_GUIDE.md](CI_CD_GUIDE.md).

