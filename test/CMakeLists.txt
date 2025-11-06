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
if (NOT DEFINED ENABLE_BENCHMARKS)
  option(ENABLE_BENCHMARKS          "Enable benchmark tests (requires Google Benchmark)" OFF)
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

add_executable(test_retry_policy test_retry_policy.cpp)
target_link_libraries(test_retry_policy PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)

add_executable(test_parameter_merger test_parameter_merger.cpp)
target_link_libraries(test_parameter_merger PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)

add_executable(test_response_parser test_response_parser.cpp)
target_link_libraries(test_response_parser PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)

add_executable(test_chat_completion_helper test_chat_completion_helper.cpp)
target_link_libraries(test_chat_completion_helper PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)

add_executable(test_secret_redaction test_secret_redaction.cpp)
target_link_libraries(test_secret_redaction PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)

add_executable(test_error_paths test_error_paths.cpp)
target_link_libraries(test_error_paths PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)
target_include_directories(test_error_paths PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

# ---- Fuzz Testing (optional) -------------------------------------------------
if (ENABLE_FUZZ)
  # Check if compiler supports libFuzzer
  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Fuzz test: RequestLogger redaction
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
    
    # Fuzz test: JSON parsing
    add_executable(fuzz_json_parsing fuzz_json_parsing.cpp)
    target_link_libraries(fuzz_json_parsing PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)
    target_compile_options(fuzz_json_parsing PRIVATE
      -fsanitize=fuzzer,address,undefined
      -O1
      -g
    )
    target_link_options(fuzz_json_parsing PRIVATE
      -fsanitize=fuzzer,address,undefined
    )
    message(STATUS "Fuzz test target 'fuzz_json_parsing' enabled")
    
    # Fuzz test: Response parsing
    add_executable(fuzz_response_parsing fuzz_response_parsing.cpp)
    target_link_libraries(fuzz_response_parsing PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)
    target_compile_options(fuzz_response_parsing PRIVATE
      -fsanitize=fuzzer,address,undefined
      -O1
      -g
    )
    target_link_options(fuzz_response_parsing PRIVATE
      -fsanitize=fuzzer,address,undefined
    )
    message(STATUS "Fuzz test target 'fuzz_response_parsing' enabled")
    
    # Fuzz test: Parameter merging
    add_executable(fuzz_parameter_merging fuzz_parameter_merging.cpp)
    target_link_libraries(fuzz_parameter_merging PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)
    target_compile_options(fuzz_parameter_merging PRIVATE
      -fsanitize=fuzzer,address,undefined
      -O1
      -g
    )
    target_link_options(fuzz_parameter_merging PRIVATE
      -fsanitize=fuzzer,address,undefined
    )
    message(STATUS "Fuzz test target 'fuzz_parameter_merging' enabled")
  else()
    message(STATUS "Fuzz testing requires Clang compiler (libFuzzer support)")
    message(STATUS "ENABLE_FUZZ=ON ignored (current compiler: ${CMAKE_CXX_COMPILER_ID})")
  endif()
endif()

# ---- Benchmark Testing (optional) ---------------------------------------------
if (ENABLE_BENCHMARKS)
  # Try to find Google Benchmark
  find_package(benchmark QUIET)
  
  if (NOT benchmark_FOUND)
    # Check if FetchContent is disabled (inherit from parent if available)
    get_property(_disable_fetchcontent GLOBAL PROPERTY LLM_DISABLE_FETCHCONTENT)
    if (NOT DEFINED _disable_fetchcontent)
      # Try to get from parent scope or cache
      if (DEFINED LLM_DISABLE_FETCHCONTENT)
        set(_disable_fetchcontent ${LLM_DISABLE_FETCHCONTENT})
      else()
        set(_disable_fetchcontent OFF)
      endif()
    endif()
    
    if (_disable_fetchcontent)
      message(FATAL_ERROR "benchmark not found and LLM_DISABLE_FETCHCONTENT=ON. Set ENABLE_BENCHMARKS=OFF or install Google Benchmark.")
    endif()
    
    include(FetchContent)
    message(STATUS "Google Benchmark not found; fetching with FetchContent")
    
    FetchContent_Declare(
      benchmark
      GIT_REPOSITORY https://github.com/google/benchmark.git
      GIT_TAG v1.8.3
    )
    
    # Disable benchmark tests and examples
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
    
    FetchContent_MakeAvailable(benchmark)
    message(STATUS "Google Benchmark fetched and configured")
  else()
    message(STATUS "Google Benchmark found via package config")
  endif()
  
  # Benchmark: ParameterMerger
  add_executable(benchmark_parameter_merger benchmark_parameter_merger.cpp)
  target_link_libraries(benchmark_parameter_merger PRIVATE 
    LLMEngine 
    llmengine::compile_options 
    nlohmann_json::nlohmann_json
    benchmark::benchmark
    benchmark::benchmark_main)
  
  # Benchmark: ResponseParser
  add_executable(benchmark_response_parser benchmark_response_parser.cpp)
  target_link_libraries(benchmark_response_parser PRIVATE 
    LLMEngine 
    llmengine::compile_options 
    nlohmann_json::nlohmann_json
    benchmark::benchmark
    benchmark::benchmark_main)
  
  # Benchmark: RequestLogger (redaction)
  add_executable(benchmark_request_logger benchmark_request_logger.cpp)
  target_link_libraries(benchmark_request_logger PRIVATE 
    LLMEngine 
    llmengine::compile_options 
    nlohmann_json::nlohmann_json
    benchmark::benchmark
    benchmark::benchmark_main)
  
  # Benchmark: JSON operations
  add_executable(benchmark_json_ops benchmark_json_ops.cpp)
  target_link_libraries(benchmark_json_ops PRIVATE 
    LLMEngine 
    llmengine::compile_options 
    nlohmann_json::nlohmann_json
    benchmark::benchmark
    benchmark::benchmark_main)
  
  message(STATUS "Benchmark targets enabled: benchmark_parameter_merger, benchmark_response_parser, benchmark_request_logger, benchmark_json_ops")
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
  add_test(NAME test_retry_policy COMMAND test_retry_policy)
  add_test(NAME test_parameter_merger COMMAND test_parameter_merger)
  add_test(NAME test_response_parser COMMAND test_response_parser)
  add_test(NAME test_chat_completion_helper COMMAND test_chat_completion_helper)
  add_test(NAME test_secret_redaction COMMAND test_secret_redaction)
  add_test(NAME test_error_paths COMMAND test_error_paths)
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
