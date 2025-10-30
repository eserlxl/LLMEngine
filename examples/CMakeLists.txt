# Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This CMake file configures building the LLMEngine example programs, and is
# licensed under the GNU General Public License v3.0 or later.
# See the LICENSE file in the project root for details.

cmake_minimum_required(VERSION 3.16)

# ---- Project -----------------------------------------------------------------
project(LLMEngineExamples
  DESCRIPTION "Example programs demonstrating LLMEngine capabilities"
  LANGUAGES CXX)

# Build system quality-of-life
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# C++ standard (no compiler extensions)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Options (inherit from root if present; otherwise define defaults) ---------
if (NOT DEFINED DEBUG_MODE)
  option(DEBUG_MODE                 "Enable debug build flags"                       OFF)
endif()
if (NOT DEFINED PERFORMANCE_BUILD)
  option(PERFORMANCE_BUILD          "Enable performance-optimized flags"             ON)
endif()
if (NOT DEFINED WARNING_MODE)
  option(WARNING_MODE               "Enable extra warnings"                          ON)
endif()
if (NOT DEFINED ENABLE_NATIVE_OPTIMIZATION)
  option(ENABLE_NATIVE_OPTIMIZATION "Use -march=native/-mtune=native in performance" OFF)
endif()
if (NOT DEFINED ENABLE_SANITIZERS)
  option(ENABLE_SANITIZERS          "Enable Address/Undefined sanitizers in debug"    OFF)
endif()
if (NOT DEFINED ENABLE_CLANG_TIDY)
  option(ENABLE_CLANG_TIDY          "Run clang-tidy if available"                     OFF)
endif()

# Put built binaries under <build>/bin (and per-config subdirs)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
foreach(cfg IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
  string(TOUPPER "${cfg}" CFG_UP)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_UP} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${cfg}")
endforeach()

# ---- Dependencies ------------------------------------------------------------
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(cpr REQUIRED)

# Try to consume LLMEngine target if available (when included from the root build)
# Otherwise, provide a local imported target pointing to the sibling build output
if (NOT TARGET LLMEngine)
  # First try a package (if installed system-wide)
  find_package(LLMEngine QUIET)
endif()

if (NOT TARGET LLMEngine)
  # Fallback to sibling build artifact for standalone example builds
  set(_LLMENGINE_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../build/libLLMEngine.a")
  if (EXISTS "${_LLMENGINE_LIB_PATH}")
    add_library(LLMEngine STATIC IMPORTED)
    set_target_properties(LLMEngine PROPERTIES
      IMPORTED_LOCATION "${_LLMENGINE_LIB_PATH}")
    target_include_directories(LLMEngine INTERFACE
      "${CMAKE_CURRENT_SOURCE_DIR}/../src"
      "${CMAKE_CURRENT_SOURCE_DIR}/../build/generated_include")
  else()
    message(FATAL_ERROR "LLMEngine target not found. Build the root project first or install LLMEngine.")
  endif()
endif()

# ---- Include directories ------------------------------------------------------
# Not needed: targets propagate include paths

# ---- Common build flags (mirror root CMake) -----------------------------------
function(apply_common_build_flags target_name)
  if (MSVC)
    if (DEBUG_MODE)
      target_compile_options(${target_name} PRIVATE /Od /Z7)
      target_compile_definitions(${target_name} PRIVATE DEBUG)
    elseif (PERFORMANCE_BUILD)
      target_compile_options(${target_name} PRIVATE /O2)
      target_compile_definitions(${target_name} PRIVATE NDEBUG)
    else()
      target_compile_options(${target_name} PRIVATE /O2 /Z7)
    endif()
  else()
    target_compile_options(${target_name} PRIVATE -fstack-protector-strong)
    target_link_options(${target_name} PRIVATE -Wl,-z,relro,-z,now)
    if (DEBUG_MODE)
      target_compile_options(${target_name} PRIVATE -O0 -g -fno-omit-frame-pointer)
      target_compile_definitions(${target_name} PRIVATE DEBUG)
      if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_definitions(${target_name} PRIVATE _GLIBCXX_ASSERTIONS)
      endif()
      if (ENABLE_SANITIZERS)
        target_compile_options(${target_name} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target_name} PRIVATE -fsanitize=address,undefined)
      endif()
    elseif (PERFORMANCE_BUILD)
      target_compile_options(${target_name} PRIVATE -O3)
      if (ENABLE_NATIVE_OPTIMIZATION)
        target_compile_options(${target_name} PRIVATE -march=native -mtune=native)
      endif()
      target_compile_definitions(${target_name} PRIVATE NDEBUG _FORTIFY_SOURCE=2)
    else()
      target_compile_options(${target_name} PRIVATE -O2 -g -fno-omit-frame-pointer)
      target_compile_definitions(${target_name} PRIVATE _FORTIFY_SOURCE=2)
    endif()
    if (WARNING_MODE)
      target_compile_options(${target_name} PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wnon-virtual-dtor
        -Wold-style-cast -Woverloaded-virtual -Wnull-dereference -Wformat=2 -Wimplicit-fallthrough)
      if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${target_name} PRIVATE -Wduplicated-cond -Wlogical-op -Wuseless-cast)
      endif()
      if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${target_name} PRIVATE -Wextra-semi -Wno-c99-extensions)
      endif()
    endif()
  endif()
endfunction()

# Optional clang-tidy
if (ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY_EXE NAMES clang-tidy)
  if (CLANG_TIDY_EXE)
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE};-warnings-as-errors=*")
    message(STATUS "clang-tidy enabled: ${CLANG_TIDY_EXE}")
  else()
    message(WARNING "ENABLE_CLANG_TIDY is ON but clang-tidy not found")
  endif()
endif()

# ---- Link directories --------------------------------------------------------
# Not needed with imported targets

# ---- Executables -------------------------------------------------------------
add_executable(chatbot chatbot.cpp)
target_link_libraries(chatbot
  PRIVATE
    LLMEngine
    cpr::cpr
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto)
apply_common_build_flags(chatbot)

add_executable(code_analyzer code_analyzer.cpp)
target_link_libraries(code_analyzer
  PRIVATE
    LLMEngine
    cpr::cpr
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto)
apply_common_build_flags(code_analyzer)

add_executable(text_processor text_processor.cpp)
target_link_libraries(text_processor
  PRIVATE
    LLMEngine
    cpr::cpr
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto)
apply_common_build_flags(text_processor)

add_executable(file_analyzer file_analyzer.cpp)
target_link_libraries(file_analyzer
  PRIVATE
    LLMEngine
    cpr::cpr
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto)
apply_common_build_flags(file_analyzer)

add_executable(translator translator.cpp)
target_link_libraries(translator
  PRIVATE
    LLMEngine
    cpr::cpr
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto)
apply_common_build_flags(translator)

add_executable(logo_generator logo_generator.cpp)
target_link_libraries(logo_generator
  PRIVATE
    LLMEngine
    cpr::cpr
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto)
apply_common_build_flags(logo_generator)

add_executable(image_generator image_generator.cpp)
target_link_libraries(image_generator
  PRIVATE
    LLMEngine
    cpr::cpr
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto)
apply_common_build_flags(image_generator)

add_executable(readme_chatbot readme_chatbot.cpp)
target_link_libraries(readme_chatbot
  PRIVATE
    LLMEngine
    cpr::cpr
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto)
apply_common_build_flags(readme_chatbot)

# ---- Summary -----------------------------------------------------------------
message(STATUS "========== Examples Build Configuration ==========")
message(STATUS "CMAKE_CXX_COMPILER       : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "CMAKE_BUILD_TYPE         : ${CMAKE_BUILD_TYPE}")
message(STATUS "DEBUG_MODE               : ${DEBUG_MODE}")
message(STATUS "PERFORMANCE_BUILD        : ${PERFORMANCE_BUILD}")
message(STATUS "WARNING_MODE             : ${WARNING_MODE}")
message(STATUS "ENABLE_NATIVE_OPTIMIZATION: ${ENABLE_NATIVE_OPTIMIZATION}")
message(STATUS "ENABLE_SANITIZERS        : ${ENABLE_SANITIZERS}")
message(STATUS "ENABLE_CLANG_TIDY        : ${ENABLE_CLANG_TIDY}")
message(STATUS "Runtime output directory : ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "========================================")
message(STATUS "Examples configuration complete")
