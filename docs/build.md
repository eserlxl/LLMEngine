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

## 🚀 Build Acceleration with ccache

ccache (compiler cache) can significantly speed up rebuilds by caching compilation results. LLMEngine's CMake configuration automatically detects and uses ccache if available.

### Local Setup

**Install ccache:**

```bash
# Ubuntu/Debian
sudo apt install ccache

# Arch Linux
sudo pacman -S ccache

# macOS (Homebrew)
brew install ccache
```

**Configure ccache (optional):**

```bash
# Set cache size (default: 5GB)
ccache -M 10G

# Show statistics
ccache -s
```

CMake will automatically use ccache if it's found in your PATH. No additional configuration needed.

### CI Usage

CI runners automatically use ccache when available. The cache is preserved between builds to accelerate compilation.

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

### Hermetic and Offline Builds

LLMEngine uses CMake's `FetchContent` to download dependencies (`nlohmann_json` and `cpr`) when system packages are not available. For reproducible builds, CI caching, or offline environments, you can control how dependencies are fetched.

#### Using CPM Source Cache

[CPM](https://github.com/cpm-cmake/CPM.cmake) provides a source cache mechanism that can speed up builds and ensure reproducibility. Set the `CPM_SOURCE_CACHE` environment variable to enable caching:

```bash
# Set cache directory (shared across projects using CPM)
export CPM_SOURCE_CACHE=$HOME/.cache/cpm

# Build - dependencies will be cached here
cmake -S . -B build
cmake --build build
```

**Benefits:**
- Dependencies are downloaded once and reused across builds
- Faster subsequent builds (no re-download)
- Can be shared across multiple projects
- Useful for CI environments to cache dependencies

**CI Example (GitHub Actions):**
```yaml
- name: Set up CPM cache
  uses: actions/cache@v3
  with:
    path: ~/.cache/cpm
    key: cpm-${{ runner.os }}-${{ hashFiles('CMakeLists.txt') }}
    restore-keys: |
      cpm-${{ runner.os }}-

- name: Configure
  env:
    CPM_SOURCE_CACHE: ~/.cache/cpm
  run: cmake -S . -B build
```

#### Fully Disconnected (Offline) Builds

For builds that must work without network access, you can pre-populate the FetchContent cache:

```bash
# First, do a connected build to populate the cache
cmake -S . -B build_connected
cmake --build build_connected

# Then, in an offline environment, use CMAKE_FETCHCONTENT_FULLY_DISCONNECTED
cmake -S . -B build_offline \
  -DCMAKE_FETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build build_offline
```

**How it works:**
- `CMAKE_FETCHCONTENT_FULLY_DISCONNECTED=ON` prevents any network requests during configuration
- CMake will only use already-populated FetchContent cache entries
- If dependencies are missing from cache, configuration will fail (as expected in offline mode)

**Alternative: Use system packages**

For truly offline builds, prefer system-installed dependencies:

```bash
# Install dependencies via package manager
sudo apt install nlohmann-json3-dev libcpr-dev

# Build with system dependencies only
cmake -S . -B build \
  -DLLM_DISABLE_FETCHCONTENT=ON
```

This approach requires no network access and ensures reproducible builds using system package versions.

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

For more details on build options and their interactions, see [CMakeLists.txt](../CMakeLists.txt) and [docs/ci_cd_guide.md](ci_cd_guide.md).

