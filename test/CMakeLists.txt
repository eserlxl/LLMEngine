# Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This CMake file configures building the LLMEngine test suite, and is
# licensed under the GNU General Public License v3.0 or later.
# See the LICENSE file in the project root for details.

cmake_minimum_required(VERSION 3.20)

# ---- Project -----------------------------------------------------------------
project(LLMEngineTest
  DESCRIPTION "Test programs for LLMEngine"
  LANGUAGES CXX)

# Build system quality-of-life
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# C++ standard (no compiler extensions) - align with library requirement
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Options (inherit from root if present; otherwise define defaults) ---------
if (NOT DEFINED DEBUG_MODE)
  option(DEBUG_MODE                 "Enable debug build flags"                       OFF)
endif()
if (NOT DEFINED PERFORMANCE_BUILD)
  option(PERFORMANCE_BUILD          "Enable performance-optimized flags"             OFF)
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
if (NOT DEFINED ENABLE_FUZZ)
  option(ENABLE_FUZZ                "Enable fuzz testing targets (requires libFuzzer)" OFF)
endif()

# Use default generator directories; avoid overriding global runtime output dir

# ---- Dependencies ------------------------------------------------------------
# Prefer consuming the build-tree package export if the root was built
if (EXISTS "${PROJECT_BINARY_DIR}/LLMEngineTargets.cmake")
  list(PREPEND CMAKE_PREFIX_PATH "${PROJECT_BINARY_DIR}")
endif()

# Try to consume LLMEngine package first (from build tree or system install)
find_package(LLMEngine CONFIG QUIET)

if (TARGET LLMEngine)
  # Transitive deps propagate; no extra package lookups needed here.
else()
  # Standalone fallback: require headers/libs used directly in tests
  find_package(OpenSSL REQUIRED)
  find_package(nlohmann_json REQUIRED)
  if (NOT TARGET cpr::cpr AND NOT TARGET cpr)
    find_package(cpr REQUIRED)
  endif()
  if (NOT TARGET cpr::cpr AND TARGET cpr)
    add_library(cpr::cpr ALIAS cpr)
  endif()

  # Provide a local imported LLMEngine target from sibling build output
  set(_LLMENGINE_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../build/libLLMEngine.a")
  if (EXISTS "${_LLMENGINE_LIB_PATH}")
    add_library(LLMEngine STATIC IMPORTED)
    set_target_properties(LLMEngine PROPERTIES IMPORTED_LOCATION "${_LLMENGINE_LIB_PATH}")
    target_include_directories(LLMEngine INTERFACE
      "${CMAKE_CURRENT_SOURCE_DIR}/../include"
      "${CMAKE_CURRENT_SOURCE_DIR}/../build/generated_include"
      "${CMAKE_CURRENT_SOURCE_DIR}/../build/generated_include/LLMEngine")
    # Propagate required link dependencies when consuming the imported static lib
    set_target_properties(LLMEngine PROPERTIES
      INTERFACE_LINK_LIBRARIES "cpr::cpr;nlohmann_json::nlohmann_json")
    # Provide a local interface target for compile options to satisfy link lines
    add_library(llmengine_compile_options INTERFACE)
    add_library(llmengine::compile_options ALIAS llmengine_compile_options)
    # Expose nlohmann_json headers if fetched by the root
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../build/_deps/nlohmann_json-src/single_include")
      target_include_directories(LLMEngine INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/../build/_deps/nlohmann_json-src/single_include")
    endif()
  else()
    message(FATAL_ERROR "LLMEngine target not found. Build the root project first or install LLMEngine.")
  endif()
endif()

# ---- Include directories ------------------------------------------------------
# Not needed: link to LLMEngine propagates includes

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

# ---- Common build flags (mirror root CMake) -----------------------------------
# Removed: tests now rely on llmengine::compile_options interface target for common flags

# ---- Test executables ---------------------------------------------------------
add_executable(test_llmengine test.cpp)
target_link_libraries(test_llmengine
    PRIVATE
    LLMEngine
    llmengine::compile_options
    cpr::cpr
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto)

add_executable(test_api test_api.cpp)
target_link_libraries(test_api
    PRIVATE
    LLMEngine
    llmengine::compile_options
    cpr::cpr
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto)

# New unit tests
add_library(test_support_fake STATIC
  support/FakeAPIClient.cpp)
target_include_directories(test_support_fake PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_support_fake PUBLIC nlohmann_json::nlohmann_json)

add_executable(test_api_config_manager test_api_config_manager.cpp)
target_link_libraries(test_api_config_manager PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)

add_executable(test_api_client_factory test_api_client_factory.cpp)
target_link_libraries(test_api_client_factory PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)

add_executable(test_provider_type_mapping test_provider_type_mapping.cpp)
target_link_libraries(test_provider_type_mapping PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)

add_executable(test_llmengine_construction test_llmengine_construction.cpp)
target_link_libraries(test_llmengine_construction PRIVATE LLMEngine llmengine::compile_options test_support_fake nlohmann_json::nlohmann_json)

# Higher-level integration tests
add_executable(test_integration test_integration.cpp)
target_link_libraries(test_integration PRIVATE LLMEngine llmengine::compile_options test_support_fake nlohmann_json::nlohmann_json)

# New tests (Phase 2)
add_executable(test_api_config_parsing test_api_config_parsing.cpp)
target_link_libraries(test_api_config_parsing PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)

add_executable(test_provider_mapping_extended test_provider_mapping_extended.cpp)
target_link_libraries(test_provider_mapping_extended PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)

add_executable(test_tempdir_hardening test_tempdir_hardening.cpp)
target_link_libraries(test_tempdir_hardening PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)

add_executable(test_request_logger test_request_logger.cpp)
target_link_libraries(test_request_logger PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)

# ---- Fuzz Testing (optional) -------------------------------------------------
if (ENABLE_FUZZ)
  # Check if compiler supports libFuzzer
  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_executable(fuzz_redactor fuzz_redactor.cpp)
    target_link_libraries(fuzz_redactor PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)
    target_compile_options(fuzz_redactor PRIVATE
      -fsanitize=fuzzer,address,undefined
      -O1
      -g
    )
    target_link_options(fuzz_redactor PRIVATE
      -fsanitize=fuzzer,address,undefined
    )
    message(STATUS "Fuzz test target 'fuzz_redactor' enabled (requires libFuzzer)")
  else()
    message(STATUS "Fuzz testing requires Clang compiler (libFuzzer support)")
    message(STATUS "ENABLE_FUZZ=ON ignored (current compiler: ${CMAKE_CXX_COMPILER_ID})")
  endif()
endif()

# ---- CTest Integration --------------------------------------------------------
# Register tests with CTest for automatic execution
if (BUILD_TESTING)
  add_test(NAME test_llmengine COMMAND test_llmengine)
  add_test(NAME test_api COMMAND test_api)
  add_test(NAME test_api_config_manager COMMAND test_api_config_manager)
  add_test(NAME test_api_client_factory COMMAND test_api_client_factory)
  add_test(NAME test_provider_type_mapping COMMAND test_provider_type_mapping)
  add_test(NAME test_llmengine_construction COMMAND test_llmengine_construction)
  add_test(NAME test_integration COMMAND test_integration)
  add_test(NAME test_api_config_parsing COMMAND test_api_config_parsing)
  add_test(NAME test_provider_mapping_extended COMMAND test_provider_mapping_extended)
  add_test(NAME test_tempdir_hardening COMMAND test_tempdir_hardening)
  add_test(NAME test_request_logger COMMAND test_request_logger)
endif()

# ---- Target properties --------------------------------------------------------
set_target_properties(test_llmengine PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/build")

set_target_properties(test_api PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/build")

# ---- Summary -----------------------------------------------------------------
message(STATUS "========== Tests Build Configuration ==========")
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
message(STATUS "Test configuration complete")
