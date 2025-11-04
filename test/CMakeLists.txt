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

# Use default generator directories; avoid overriding global runtime output dir

# ---- Dependencies ------------------------------------------------------------
# Prefer consuming the build-tree package export if the root was built
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../build/${CMAKE_PROJECT_NAME}Targets.cmake" OR
    EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../build/LLMEngineTargets.cmake")
  list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../build")
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
    if (ENABLE_COVERAGE)
      if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
          target_compile_options(${target_name} PRIVATE -fprofile-instr-generate -fcoverage-mapping)
          target_link_options(${target_name} PRIVATE -fprofile-instr-generate -fcoverage-mapping)
        elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
          target_compile_options(${target_name} PRIVATE --coverage -O0 -g)
          target_link_options(${target_name} PRIVATE --coverage)
        endif()
      endif()
    endif()
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
apply_common_build_flags(test_llmengine)

add_executable(test_api test_api.cpp)
target_link_libraries(test_api
    PRIVATE
    LLMEngine
    llmengine::compile_options
    cpr::cpr
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto)
apply_common_build_flags(test_api)

# New unit tests
add_library(test_support_fake STATIC
  support/FakeAPIClient.cpp)
target_include_directories(test_support_fake PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_support_fake PUBLIC nlohmann_json::nlohmann_json)
apply_common_build_flags(test_support_fake)

add_executable(test_api_config_manager test_api_config_manager.cpp)
target_link_libraries(test_api_config_manager PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)
apply_common_build_flags(test_api_config_manager)

add_executable(test_api_client_factory test_api_client_factory.cpp)
target_link_libraries(test_api_client_factory PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)
apply_common_build_flags(test_api_client_factory)

add_executable(test_provider_type_mapping test_provider_type_mapping.cpp)
target_link_libraries(test_provider_type_mapping PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)
apply_common_build_flags(test_provider_type_mapping)

add_executable(test_llmengine_construction test_llmengine_construction.cpp)
target_link_libraries(test_llmengine_construction PRIVATE LLMEngine llmengine::compile_options test_support_fake nlohmann_json::nlohmann_json)
apply_common_build_flags(test_llmengine_construction)

# New tests (Phase 2)
add_executable(test_api_config_parsing test_api_config_parsing.cpp)
target_link_libraries(test_api_config_parsing PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)
apply_common_build_flags(test_api_config_parsing)

add_executable(test_output_processor_sections test_output_processor_sections.cpp)
target_link_libraries(test_output_processor_sections PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)
apply_common_build_flags(test_output_processor_sections)

add_executable(test_provider_mapping_extended test_provider_mapping_extended.cpp)
target_link_libraries(test_provider_mapping_extended PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)
apply_common_build_flags(test_provider_mapping_extended)

# ---- CTest Integration --------------------------------------------------------
# Register tests with CTest for automatic execution
if (BUILD_TESTING)
  add_test(NAME test_llmengine COMMAND test_llmengine)
  add_test(NAME test_api COMMAND test_api)
  add_test(NAME test_api_config_manager COMMAND test_api_config_manager)
  add_test(NAME test_api_client_factory COMMAND test_api_client_factory)
  add_test(NAME test_provider_type_mapping COMMAND test_provider_type_mapping)
  add_test(NAME test_llmengine_construction COMMAND test_llmengine_construction)
  add_test(NAME test_api_config_parsing COMMAND test_api_config_parsing)
  add_test(NAME test_output_processor_sections COMMAND test_output_processor_sections)
  add_test(NAME test_provider_mapping_extended COMMAND test_provider_mapping_extended)
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
