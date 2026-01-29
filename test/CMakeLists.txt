# Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This CMake file configures building the LLMEngine test suite, and is
# licensed under the GNU General Public License v3.0 or later.
# See the LICENSE file in the project root for details.

cmake_minimum_required(VERSION 3.31)

# ---- Project -----------------------------------------------------------------
project(LLMEngineTest
  DESCRIPTION "Test programs for LLMEngine"
  LANGUAGES CXX)

# Build system quality-of-life
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- Options (inherit from root; only define if not already set) ---------
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

# Inherit C++ standard settings from parent if available, otherwise use same defaults
if (NOT DEFINED CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 23)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif()

# ---- Dependencies ------------------------------------------------------------
find_package(OpenSSL REQUIRED)
find_package(GTest REQUIRED)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(UNISTRING QUIET libunistring)
endif()

if(UNISTRING_FOUND AND UNISTRING_LIBRARIES)
  message(STATUS "Found libunistring via pkg-config: ${UNISTRING_LIBRARIES}")
  add_library(unistring INTERFACE)
  target_link_libraries(unistring INTERFACE ${UNISTRING_LIBRARIES})
  set(UNISTRING_TARGET unistring)
else()
  find_library(UNISTRING_LIBRARY
    NAMES unistring libunistring
    PATHS /usr/lib /usr/local/lib /lib /usr/lib/x86_64-linux-gnu /usr/lib64
  )
  if(UNISTRING_LIBRARY)
    message(STATUS "Found libunistring: ${UNISTRING_LIBRARY}")
    add_library(unistring INTERFACE)
    target_link_libraries(unistring INTERFACE ${UNISTRING_LIBRARY})
    set(UNISTRING_TARGET unistring)
  else()
    message(STATUS "libunistring not found (may not be needed on this system)")
    set(UNISTRING_TARGET "")
  endif()
endif()

if (TARGET LLMEngine)
  message(STATUS "Using LLMEngine target from parent scope")
else()
  # Simplified search/import logic for standalone builds
  find_package(LLMEngine CONFIG QUIET)
  if (TARGET LLMEngine)
    message(STATUS "Found LLMEngine via package config")
  else()
    # Fallback manual import
    find_package(nlohmann_json REQUIRED)
    if (NOT TARGET cpr::cpr AND NOT TARGET cpr)
      find_package(cpr REQUIRED)
    endif()
    if (NOT TARGET cpr::cpr AND TARGET cpr)
      add_library(cpr::cpr ALIAS cpr)
    endif()
    
    # Assuming standard build directory structure for standalone tests
    set(_possible_build_dirs "${CMAKE_CURRENT_SOURCE_DIR}/../build")
    foreach(_build_dir IN LISTS _possible_build_dirs)
      if (EXISTS "${_build_dir}/libLLMEngine.a")
        add_library(LLMEngine STATIC IMPORTED)
        set_target_properties(LLMEngine PROPERTIES IMPORTED_LOCATION "${_build_dir}/libLLMEngine.a")
        target_include_directories(LLMEngine INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/../include" "${_build_dir}/generated_include")
        set_target_properties(LLMEngine PROPERTIES INTERFACE_LINK_LIBRARIES "cpr::cpr;nlohmann_json::nlohmann_json")
        add_library(llmengine_compile_options INTERFACE)
        add_library(llmengine::compile_options ALIAS llmengine_compile_options)
        message(STATUS "Imported LLMEngine manually")
        break()
      endif()
    endforeach()
  endif()
endif()

# Optional clang-tidy
if (ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY_EXE NAMES clang-tidy)
  if (CLANG_TIDY_EXE)
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE};-warnings-as-errors=*")
    message(STATUS "clang-tidy enabled: ${CLANG_TIDY_EXE}")
  endif()
endif()

# ---- Sanitizer flags for test targets -----------------------------------------
set(_TEST_SANITIZER_TARGET "")
if (DEBUG_MODE AND ENABLE_SANITIZERS AND NOT MSVC)
  set(_test_san_list "")
  if (LLM_ENABLE_ASAN OR ENABLE_SANITIZERS)
    list(APPEND _test_san_list "address")
  endif()
  if (LLM_ENABLE_UBSAN OR ENABLE_SANITIZERS)
    list(APPEND _test_san_list "undefined")
  endif()
  if (LLM_ENABLE_MSAN)
    list(APPEND _test_san_list "memory")
  endif()
  if (LLM_ENABLE_TSAN)
    list(APPEND _test_san_list "thread")
  endif()
  if (_test_san_list)
    string(REPLACE ";" "," _test_san_flags "${_test_san_list}")
    add_library(test_sanitizer_flags INTERFACE)
    target_compile_options(test_sanitizer_flags INTERFACE
      $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-fsanitize=${_test_san_flags}>
      $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-fno-omit-frame-pointer>)
    target_link_options(test_sanitizer_flags INTERFACE
      $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-fsanitize=${_test_san_flags}>)
    set(_TEST_SANITIZER_TARGET "test_sanitizer_flags")
    message(STATUS "Sanitizer flags enabled for test targets: ${_test_san_flags}")
  endif()
endif()

# Helper function
function(apply_test_sanitizer_flags target_name)
  if (TARGET test_sanitizer_flags)
    target_link_libraries(${target_name} PRIVATE test_sanitizer_flags)
  endif()
  if (UNISTRING_TARGET)
    target_link_libraries(${target_name} PRIVATE ${UNISTRING_TARGET})
  endif()
endfunction()

# ---- Main Helper Function ----------------------------------------------------
function(llmengine_test name source)
  add_executable(${name} ${source})
  target_link_libraries(${name}
    PRIVATE
    LLMEngine
    llmengine::compile_options
    nlohmann_json::nlohmann_json
    GTest::gtest
    GTest::gmock
    GTest::gtest_main
    ${ARGN} # Additional dependencies
  )
  apply_test_sanitizer_flags(${name})
  
  if (BUILD_TESTING)
    add_test(NAME ${name} COMMAND ${name})
  endif()
endfunction()

# ---- Support Libraries -------------------------------------------------------
add_library(test_support_fake STATIC support/FakeAPIClient.cpp)
target_include_directories(test_support_fake PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_support_fake PUBLIC LLMEngine nlohmann_json::nlohmann_json)
apply_test_sanitizer_flags(test_support_fake)

# ---- Test Executables --------------------------------------------------------
llmengine_test(test_llmengine test.cpp cpr::cpr OpenSSL::SSL OpenSSL::Crypto)
set_target_properties(test_llmengine PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/build")

llmengine_test(test_api test_api.cpp cpr::cpr OpenSSL::SSL OpenSSL::Crypto)
set_target_properties(test_api PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/build")

llmengine_test(test_api_config_manager test_api_config_manager.cpp)
llmengine_test(test_api_config_manager_thread_safety test_api_config_manager_thread_safety.cpp)
llmengine_test(test_request_context_builder_thread_safety test_request_context_builder_thread_safety.cpp)
llmengine_test(test_api_client_factory test_api_client_factory.cpp)
llmengine_test(test_provider_type_mapping test_provider_type_mapping.cpp)
llmengine_test(test_llmengine_construction test_llmengine_construction.cpp test_support_fake)
llmengine_test(test_integration test_integration.cpp test_support_fake)

llmengine_test(test_api_config_parsing test_api_config_parsing.cpp)
if (BUILD_TESTING)
  set_tests_properties(test_api_config_parsing PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

llmengine_test(test_provider_mapping_extended test_provider_mapping_extended.cpp)
llmengine_test(test_tempdir_hardening test_tempdir_hardening.cpp)
llmengine_test(test_request_logger test_request_logger.cpp)

llmengine_test(test_retry_policy test_retry_policy.cpp cpr::cpr)
target_include_directories(test_retry_policy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

llmengine_test(test_parameter_merger test_parameter_merger.cpp)
llmengine_test(test_response_parser test_response_parser.cpp)

llmengine_test(test_chat_completion_helper test_chat_completion_helper.cpp cpr::cpr)
target_include_directories(test_chat_completion_helper PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

llmengine_test(test_secret_redaction test_secret_redaction.cpp)

llmengine_test(test_error_paths test_error_paths.cpp cpr::cpr)
target_include_directories(test_error_paths PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

llmengine_test(test_async_stream test_async_stream.cpp test_support_fake)
llmengine_test(test_async_lifetime test_async_lifetime.cpp)
llmengine_test(test_result_extensions test_result_extensions.cpp)
llmengine_test(test_new_features_iter1 test_new_features_iter1.cpp test_support_fake)
llmengine_test(test_async_streaming_iter2 test_async_streaming_iter2.cpp test_support_fake)
llmengine_test(test_features_iter2 test_features_iter2.cpp cpr::cpr test_support_fake)
target_include_directories(test_features_iter2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
llmengine_test(test_streaming_providers test_streaming_providers.cpp cpr::cpr)
target_include_directories(test_streaming_providers PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

llmengine_test(test_timeout test_timeout.cpp test_support_fake)

# Iteration 1 New Test
llmengine_test(test_request_options_builder test_request_options_builder.cpp)
llmengine_test(test_tool_builder test_tool_builder.cpp)
llmengine_test(test_parameter_propagation test_parameter_propagation.cpp)
llmengine_test(test_tool_builder_extended test_tool_builder_extended.cpp)
llmengine_test(test_streaming_metrics_fix test_streaming_metrics_fix.cpp test_support_fake)
llmengine_test(test_thread_pool test_thread_pool.cpp)
llmengine_test(test_analysis_result_extensions test_analysis_result_extensions.cpp)
llmengine_test(test_parameter_merger_extended test_parameter_merger_extended.cpp)
llmengine_test(test_llmengine_thread_safety test_llmengine_thread_safety.cpp test_support_fake)

# Iteration 1 Extensions
llmengine_test(test_llmengine_builder test_llmengine_builder.cpp)
llmengine_test(test_batch_concurrency test_batch_concurrency.cpp test_support_fake)

# Iteration 2 Refinements
llmengine_test(test_builder_refinements test_builder_refinements.cpp)
llmengine_test(test_async_cancellation test_async_cancellation.cpp test_support_fake)
llmengine_test(test_validation_and_interceptors test_validation_and_interceptors.cpp test_support_fake)
llmengine_test(test_command_executor test_command_executor.cpp)



# ---- Build All Tests Target --------------------------------------------------
if (BUILD_TESTING)
  add_custom_target(build_all_tests
    DEPENDS
      test_llmengine
      test_api
      test_api_config_manager
      test_api_config_manager_thread_safety
      test_request_context_builder_thread_safety
      test_api_client_factory
      test_provider_type_mapping
      test_llmengine_construction
      test_integration
      test_api_config_parsing
      test_provider_mapping_extended
      test_tempdir_hardening
      test_request_logger
      test_retry_policy
      test_parameter_merger
      test_response_parser
      test_chat_completion_helper
      test_secret_redaction
      test_error_paths
      test_async_stream
      test_async_lifetime
      test_result_extensions
      test_new_features_iter1
      test_async_streaming_iter2
      test_features_iter2
      test_features_iter2
      test_streaming_providers
      test_timeout
      test_request_options_builder
      test_parameter_propagation
      test_tool_builder_extended
      test_streaming_metrics_fix
      test_parameter_merger_extended
      test_llmengine_thread_safety
    COMMENT "Building all test executables"


  )
endif()

# ---- Fuzz and Benchmark (kept mostly as is but cleaned up) --------------------
# (Omitting full recreation of these specific blocks to save space if they weren't focus, 
# but per strict instructions I must output full file if I overwrite. 
# I will include them to be safe.)

if (ENABLE_FUZZ)
  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Fuzz helpers could be refactored too, but leaving them for now or using a separate function
    function(llmengine_fuzz name source)
      add_executable(${name} ${source})
      target_link_libraries(${name} PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)
      target_compile_options(${name} PRIVATE -fsanitize=fuzzer,address,undefined -O1 -g)
      target_link_options(${name} PRIVATE -fsanitize=fuzzer,address,undefined)
      message(STATUS "Fuzz test target '${name}' enabled")
    endfunction()
    
    llmengine_fuzz(fuzz_redactor fuzz_redactor.cpp)
    llmengine_fuzz(fuzz_json_parsing fuzz_json_parsing.cpp)
    llmengine_fuzz(fuzz_response_parsing fuzz_response_parsing.cpp)
    llmengine_fuzz(fuzz_parameter_merging fuzz_parameter_merging.cpp)
  else()
    message(STATUS "Fuzz testing requires Clang compiler")
  endif()
endif()

if (ENABLE_BENCHMARKS)
  find_package(benchmark QUIET)
  if (NOT benchmark_FOUND)
    # ... (FetchContent logic omitted for brevity in thought but included in output) ...
    include(FetchContent)
    FetchContent_Declare(benchmark GIT_REPOSITORY https://github.com/google/benchmark.git GIT_TAG v1.8.3)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(benchmark)
  endif()
  
  function(llmengine_benchmark name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json benchmark::benchmark benchmark::benchmark_main)
  endfunction()
  
  llmengine_benchmark(benchmark_parameter_merger benchmark_parameter_merger.cpp)
  llmengine_benchmark(benchmark_response_parser benchmark_response_parser.cpp)
  llmengine_benchmark(benchmark_request_logger benchmark_request_logger.cpp)
  llmengine_benchmark(benchmark_json_ops benchmark_json_ops.cpp)
  
  message(STATUS "Benchmark targets enabled")
endif()

message(STATUS "Test configuration complete")

add_executable(test_features_iter3 test_features_iter3.cpp)
target_link_libraries(test_features_iter3 PRIVATE LLMEngine)
