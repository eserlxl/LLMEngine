# CMake Build Rules

## Overview

This document defines CMake build system guidelines for LLMEngine. These rules ensure modern, maintainable, and efficient CMake configurations.

## CMake Version

- **Minimum**: CMake 3.20
- **Required**: CMake 3.20 or later for all features
- **Policy**: Set modern CMake policies explicitly

```cmake
cmake_minimum_required(VERSION 3.20)

# Set modern policies
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW) # FetchContent timestamp handling
endif()
```

## Modern CMake Targets

### Target-Based Configuration

- **Always use targets**: `target_*` commands, not global commands
- **Never use global commands**: `include_directories()`, `link_directories()`, `add_definitions()`
- **Interface targets**: Use for compile options and dependencies

```cmake
# Good: Target-based
add_library(MyLib src/MyLib.cpp)
target_include_directories(MyLib PUBLIC include)
target_compile_features(MyLib PUBLIC cxx_std_23)
target_link_libraries(MyLib PUBLIC SomeDep::SomeDep)

# Bad: Global commands
include_directories(include) # Don't use
link_directories(lib)        # Don't use
add_definitions(-DSOME_MACRO) # Don't use
```

### target_sources

- Use `target_sources()` to add source files
- Prefer explicit file lists over `GLOB`
- Use `PRIVATE`, `PUBLIC`, or `INTERFACE` as appropriate

```cmake
add_library(MyLib)
target_sources(MyLib
  PRIVATE
    src/MyLib.cpp
    src/Helper.cpp
  PUBLIC
    include/MyLib.hpp  # Header-only interface
)
```

### target_link_libraries

- Use `target_link_libraries()` with proper visibility:
  - **PUBLIC**: Required by consumers of this target
  - **PRIVATE**: Only needed for implementation
  - **INTERFACE**: Header-only, no implementation

```cmake
target_link_libraries(MyLib
  PUBLIC
    nlohmann_json::nlohmann_json  # Consumers need this
  PRIVATE
    SomeInternalLib                # Only implementation needs
)
```

### target_compile_features

- Use `target_compile_features()` to specify C++ standard
- Prefer over `set(CMAKE_CXX_STANDARD)`
- Use `PUBLIC` to propagate to consumers

```cmake
target_compile_features(MyLib PUBLIC cxx_std_23)
```

## Position-Independent Code

- **Always enable PIC**: Required for shared libraries
- **Set globally**: `set(CMAKE_POSITION_INDEPENDENT_CODE ON)`
- **Applies to all targets**: Both static and shared libraries

```cmake
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
```

## Export Compile Commands

- **Enable for IDEs**: Generate `compile_commands.json`
- **Required for**: clangd, LSP servers, static analysis tools

```cmake
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
```

## Warning Flags

### Per-Target Warnings

- Apply warnings per-target, not globally
- Use interface target for common warnings
- Respect user's `WARNING_MODE` option

```cmake
# Create interface target for compile options
add_library(compile_options INTERFACE)

if(WARNING_MODE)
  target_compile_options(compile_options INTERFACE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wshadow
  )
  
  if(WERROR)
    target_compile_options(compile_options INTERFACE -Werror)
  endif()
endif()

# Apply to targets
target_link_libraries(MyLib PRIVATE compile_options)
```

### Warning Flags by Compiler

- **GCC/Clang**: `-Wall -Wextra -Wpedantic -Wconversion -Wshadow`
- **MSVC**: `/W4 /permissive-`
- Use generator expressions for compiler-specific flags

```cmake
target_compile_options(MyLib PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra>
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
```

## Sanitizer Presets

### Debug Mode Sanitizers

- Enable sanitizers in Debug builds only
- Use CMake options to control sanitizers
- Apply via target options, not globally

```cmake
option(ENABLE_SANITIZERS "Enable sanitizers in Debug" OFF)
option(LLM_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(LLM_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

if(DEBUG_MODE AND (ENABLE_SANITIZERS OR LLM_ENABLE_ASAN))
  target_compile_options(MyLib PRIVATE
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-fsanitize=address>
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-fno-omit-frame-pointer>
  )
  target_link_options(MyLib PRIVATE
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-fsanitize=address>
  )
endif()
```

### Available Sanitizers

- **AddressSanitizer (ASan)**: Memory errors, use-after-free, buffer overflows
- **UndefinedBehaviorSanitizer (UBSan)**: Undefined behavior detection
- **MemorySanitizer (MSan)**: Uninitialized memory (Clang-only)
- **ThreadSanitizer (TSan)**: Data races

## CMake Options

### Standard Options

Define standard options for build configuration:

```cmake
option(BUILD_TESTING "Build tests" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(ENABLE_EXAMPLES "Build examples" ON)
option(WARNING_MODE "Enable warnings" ON)
option(WERROR "Treat warnings as errors" OFF)
option(ENABLE_SANITIZERS "Enable sanitizers in Debug" OFF)
option(ENABLE_COVERAGE "Enable coverage" OFF)
```

### Build Modes

- **DEBUG_MODE**: Debug flags (`-O0 -g`)
- **PERFORMANCE_BUILD**: Performance flags (`-O3`)
- **Default**: Release-like (`-O2 -g`)

```cmake
option(DEBUG_MODE "Debug build" OFF)
option(PERFORMANCE_BUILD "Performance build" OFF)
```

## Dependency Management

### FetchContent

- Use `FetchContent` for dependencies when system packages unavailable
- Prefer system packages via `find_package()`
- Pin versions for reproducibility

```cmake
include(FetchContent)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)

FetchContent_MakeAvailable(nlohmann_json)
```

### System Packages

- Prefer system-provided packages
- Use `find_package()` with `CONFIG` mode
- Fall back to `FetchContent` if not found

```cmake
find_package(nlohmann_json CONFIG QUIET)
if(NOT nlohmann_json_FOUND)
  # Fall back to FetchContent
endif()
```

### Package Config

- Generate `*Config.cmake` for `find_package()` support
- Use `CMakePackageConfigHelpers` for version and config files
- Export targets properly

```cmake
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/MyLibConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Config.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/MyLibConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/MyLib"
)
```

## Installation

### Install Targets

```cmake
install(TARGETS MyLib
  EXPORT MyLibTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
```

### Install Headers

```cmake
install(DIRECTORY include/MyLib/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/MyLib
  FILES_MATCHING PATTERN "*.hpp"
)
```

### Install Config Files

```cmake
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/MyLibConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/MyLibConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MyLib
)

install(EXPORT MyLibTargets
  NAMESPACE MyLib::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MyLib
)
```

## Testing Integration

- Use CTest for test discovery
- Register tests with `add_test()`
- Enable testing with `enable_testing()`

```cmake
enable_testing()

add_executable(test_my_lib test/test_my_lib.cpp)
target_link_libraries(test_my_lib PRIVATE MyLib)

add_test(NAME test_my_lib COMMAND test_my_lib)
```

## Best Practices

1. **Target-based**: Always use `target_*` commands
2. **Modern CMake**: Use CMake 3.20+ features
3. **PIC**: Always enable position-independent code
4. **Compile commands**: Export for IDE support
5. **Warnings**: Per-target, controlled by options
6. **Sanitizers**: Debug mode only, behind options
7. **Dependencies**: System packages first, FetchContent fallback
8. **Installation**: Proper CMake package config

## References

- See `60-tests.md` for test integration
- See `70-ci.md` for CI build configuration
- [Modern CMake](https://cliutils.gitlab.io/modern-cmake/)

