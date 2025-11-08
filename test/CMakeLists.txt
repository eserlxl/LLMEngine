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

# ---- Options (inherit from root; only define if not already set) ---------
# When included as subdirectory, options are already defined by root CMakeLists.txt
# When used standalone, define with same defaults as root to maintain consistency
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
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif()

# Use default generator directories; avoid overriding global runtime output dir

# ---- Dependencies ------------------------------------------------------------
# Strategy: Prefer exported targets from build tree, then installed package, then fallback
# When included as subdirectory, LLMEngine target should already exist
# When used standalone, try to find via package config or build tree export

# Find OpenSSL (required by test targets that link to OpenSSL::SSL)
find_package(OpenSSL REQUIRED)

# Find libunistring (required on some systems like Arch Linux where libidn2 depends on it)
# This is a transitive dependency of curl/cpr that may not be automatically linked
# Try pkg-config first, then fall back to find_library
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
  # Fall back to find_library
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

# First, check if we're being included as a subdirectory (target already exists)
if (TARGET LLMEngine)
  # Target already available from parent scope - nothing to do
  message(STATUS "Using LLMEngine target from parent scope")
else()
  # Try to find via build tree export (when root project was built in same build tree)
  # Check both current binary dir and parent binary dir for exported targets
  set(_possible_prefix_paths
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/.."
    "${CMAKE_SOURCE_DIR}/build"
    "${CMAKE_SOURCE_DIR}/build-msan"
    "${CMAKE_SOURCE_DIR}/build-asan"
    "${CMAKE_SOURCE_DIR}/build-ubsan"
    "${CMAKE_SOURCE_DIR}/build-tsan"
  )
  
  foreach(_prefix_path IN LISTS _possible_prefix_paths)
    if (EXISTS "${_prefix_path}/LLMEngineTargets.cmake")
      list(PREPEND CMAKE_PREFIX_PATH "${_prefix_path}")
      break()
    endif()
  endforeach()
  
  # Try to consume LLMEngine package (from build tree export or system install)
  find_package(LLMEngine CONFIG QUIET)
  
  if (TARGET LLMEngine)
    message(STATUS "Found LLMEngine via package config")
  else()
    # Last resort: try to use exported target from common parent build directories
    # This works when test is built separately but root was built in a build directory
    set(_possible_parent_build_dirs
      "${CMAKE_CURRENT_SOURCE_DIR}/../build"
      "${CMAKE_CURRENT_SOURCE_DIR}/../build-msan"
      "${CMAKE_CURRENT_SOURCE_DIR}/../build-asan"
      "${CMAKE_CURRENT_SOURCE_DIR}/../build-ubsan"
      "${CMAKE_CURRENT_SOURCE_DIR}/../build-tsan"
    )
    foreach(_parent_build_dir IN LISTS _possible_parent_build_dirs)
      if (EXISTS "${_parent_build_dir}/LLMEngineTargets.cmake")
        list(PREPEND CMAKE_PREFIX_PATH "${_parent_build_dir}")
        find_package(LLMEngine CONFIG QUIET)
        if (TARGET LLMEngine)
          message(STATUS "Found LLMEngine via exported targets in ${_parent_build_dir}")
          break()
        endif()
      endif()
    endforeach()
  endif()
  
  # If still not found, fall back to manual import (for standalone builds)
  if (NOT TARGET LLMEngine)
    message(WARNING "LLMEngine target not found via package config. Attempting manual import from build tree.")
    # Standalone fallback: require headers/libs used directly in tests
    # Note: OpenSSL is already found at the top of this file
    find_package(nlohmann_json REQUIRED)
    if (NOT TARGET cpr::cpr AND NOT TARGET cpr)
      find_package(cpr REQUIRED)
    endif()
    if (NOT TARGET cpr::cpr AND TARGET cpr)
      add_library(cpr::cpr ALIAS cpr)
    endif()

    # Try to import from build tree using multi-config aware paths
    # This supports both single-config (Debug/Release) and multi-config generators
    set(_llmengine_lib_name "libLLMEngine.a")
    if (WIN32)
      set(_llmengine_lib_name "LLMEngine.lib")
    endif()
    
    # Try multiple possible locations for the library
    # Check common build directory names (build, build-msan, build-asan, etc.)
    set(_possible_build_dirs
      "${CMAKE_CURRENT_SOURCE_DIR}/../build"
      "${CMAKE_CURRENT_SOURCE_DIR}/../build-msan"
      "${CMAKE_CURRENT_SOURCE_DIR}/../build-asan"
      "${CMAKE_CURRENT_SOURCE_DIR}/../build-ubsan"
      "${CMAKE_CURRENT_SOURCE_DIR}/../build-tsan"
      "${CMAKE_CURRENT_BINARY_DIR}/.."
    )
    
    set(_found_lib_path "")
    set(_found_build_dir "")
    foreach(_build_dir IN LISTS _possible_build_dirs)
      # Check root of build directory first
      if (EXISTS "${_build_dir}/${_llmengine_lib_name}")
        set(_found_lib_path "${_build_dir}/${_llmengine_lib_name}")
        set(_found_build_dir "${_build_dir}")
        break()
      endif()
      # Check build type subdirectories (use actual CMAKE_BUILD_TYPE if set)
      if (CMAKE_BUILD_TYPE)
        if (EXISTS "${_build_dir}/${CMAKE_BUILD_TYPE}/${_llmengine_lib_name}")
          set(_found_lib_path "${_build_dir}/${CMAKE_BUILD_TYPE}/${_llmengine_lib_name}")
          set(_found_build_dir "${_build_dir}")
          break()
        endif()
      endif()
      # Check common build type directories
      if (EXISTS "${_build_dir}/Debug/${_llmengine_lib_name}")
        set(_found_lib_path "${_build_dir}/Debug/${_llmengine_lib_name}")
        set(_found_build_dir "${_build_dir}")
        break()
      endif()
      if (EXISTS "${_build_dir}/Release/${_llmengine_lib_name}")
        set(_found_lib_path "${_build_dir}/Release/${_llmengine_lib_name}")
        set(_found_build_dir "${_build_dir}")
        break()
      endif()
    endforeach()
    
    if (_found_lib_path)
      add_library(LLMEngine STATIC IMPORTED)
      set_target_properties(LLMEngine PROPERTIES IMPORTED_LOCATION "${_found_lib_path}")
      # Use the detected build directory for include paths
      target_include_directories(LLMEngine INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/../include"
        "${_found_build_dir}/generated_include"
        "${_found_build_dir}/generated_include/LLMEngine")
      # Propagate required link dependencies when consuming the imported static lib
      set_target_properties(LLMEngine PROPERTIES
        INTERFACE_LINK_LIBRARIES "cpr::cpr;nlohmann_json::nlohmann_json")
      # Provide a local interface target for compile options to satisfy link lines
      add_library(llmengine_compile_options INTERFACE)
      add_library(llmengine::compile_options ALIAS llmengine_compile_options)
      # Expose nlohmann_json headers if fetched by the root
      if (EXISTS "${_found_build_dir}/_deps/nlohmann_json-src/single_include")
        target_include_directories(LLMEngine INTERFACE
          "${_found_build_dir}/_deps/nlohmann_json-src/single_include")
      endif()
      message(STATUS "Imported LLMEngine from ${_found_lib_path} (build dir: ${_found_build_dir})")
    else()
      message(FATAL_ERROR "LLMEngine target not found. Build the root project first, install LLMEngine, or ensure build tree is accessible.")
    endif()
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

# ---- Sanitizer flags for test targets -----------------------------------------
# When sanitizers are enabled, test targets must also be compiled/linked with sanitizer flags
# to avoid undefined references to sanitizer runtime symbols
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
    # Create an interface target for sanitizer flags that test targets can link to
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

# Helper function to apply sanitizer flags and optional dependencies to a test target
function(apply_test_sanitizer_flags target_name)
  if (TARGET test_sanitizer_flags)
    target_link_libraries(${target_name} PRIVATE test_sanitizer_flags)
  endif()
  # Add libunistring if found (needed on Arch Linux and some other systems)
  if (UNISTRING_TARGET)
    target_link_libraries(${target_name} PRIVATE ${UNISTRING_TARGET})
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
apply_test_sanitizer_flags(test_llmengine)

add_executable(test_api test_api.cpp)
target_link_libraries(test_api
    PRIVATE
    LLMEngine
    llmengine::compile_options
    cpr::cpr
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto)
apply_test_sanitizer_flags(test_api)

# New unit tests
add_library(test_support_fake STATIC
  support/FakeAPIClient.cpp)
target_include_directories(test_support_fake PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_support_fake PUBLIC LLMEngine nlohmann_json::nlohmann_json)
apply_test_sanitizer_flags(test_support_fake)

add_executable(test_api_config_manager test_api_config_manager.cpp)
target_link_libraries(test_api_config_manager PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)
apply_test_sanitizer_flags(test_api_config_manager)

add_executable(test_api_config_manager_thread_safety test_api_config_manager_thread_safety.cpp)
target_link_libraries(test_api_config_manager_thread_safety PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)
apply_test_sanitizer_flags(test_api_config_manager_thread_safety)

add_executable(test_request_context_builder_thread_safety test_request_context_builder_thread_safety.cpp)
target_link_libraries(test_request_context_builder_thread_safety PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)
apply_test_sanitizer_flags(test_request_context_builder_thread_safety)

add_executable(test_api_client_factory test_api_client_factory.cpp)
target_link_libraries(test_api_client_factory PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)
apply_test_sanitizer_flags(test_api_client_factory)

add_executable(test_provider_type_mapping test_provider_type_mapping.cpp)
target_link_libraries(test_provider_type_mapping PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)
apply_test_sanitizer_flags(test_provider_type_mapping)

add_executable(test_llmengine_construction test_llmengine_construction.cpp)
target_link_libraries(test_llmengine_construction PRIVATE LLMEngine llmengine::compile_options test_support_fake nlohmann_json::nlohmann_json)
apply_test_sanitizer_flags(test_llmengine_construction)

# Higher-level integration tests
add_executable(test_integration test_integration.cpp)
target_link_libraries(test_integration PRIVATE LLMEngine llmengine::compile_options test_support_fake nlohmann_json::nlohmann_json)
apply_test_sanitizer_flags(test_integration)

# New tests (Phase 2)
add_executable(test_api_config_parsing test_api_config_parsing.cpp)
target_link_libraries(test_api_config_parsing PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)
apply_test_sanitizer_flags(test_api_config_parsing)

add_executable(test_provider_mapping_extended test_provider_mapping_extended.cpp)
target_link_libraries(test_provider_mapping_extended PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)
apply_test_sanitizer_flags(test_provider_mapping_extended)

add_executable(test_tempdir_hardening test_tempdir_hardening.cpp)
target_link_libraries(test_tempdir_hardening PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)
apply_test_sanitizer_flags(test_tempdir_hardening)

add_executable(test_request_logger test_request_logger.cpp)
target_link_libraries(test_request_logger PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)
apply_test_sanitizer_flags(test_request_logger)

add_executable(test_retry_policy test_retry_policy.cpp)
target_link_libraries(test_retry_policy PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json cpr::cpr)
target_include_directories(test_retry_policy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
apply_test_sanitizer_flags(test_retry_policy)

add_executable(test_parameter_merger test_parameter_merger.cpp)
target_link_libraries(test_parameter_merger PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)
apply_test_sanitizer_flags(test_parameter_merger)

add_executable(test_response_parser test_response_parser.cpp)
target_link_libraries(test_response_parser PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)
apply_test_sanitizer_flags(test_response_parser)

add_executable(test_chat_completion_helper test_chat_completion_helper.cpp)
target_link_libraries(test_chat_completion_helper PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json cpr::cpr)
target_include_directories(test_chat_completion_helper PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
apply_test_sanitizer_flags(test_chat_completion_helper)

add_executable(test_secret_redaction test_secret_redaction.cpp)
target_link_libraries(test_secret_redaction PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json)
apply_test_sanitizer_flags(test_secret_redaction)

add_executable(test_error_paths test_error_paths.cpp)
target_link_libraries(test_error_paths PRIVATE LLMEngine llmengine::compile_options nlohmann_json::nlohmann_json cpr::cpr)
target_include_directories(test_error_paths PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
apply_test_sanitizer_flags(test_error_paths)

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
  add_test(NAME test_api_config_manager_thread_safety COMMAND test_api_config_manager_thread_safety)
  add_test(NAME test_request_context_builder_thread_safety COMMAND test_request_context_builder_thread_safety)
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
  
  # Create a custom target that depends on all test executables
  # This allows building all tests with a single target even when EXCLUDE_FROM_ALL is used
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
    COMMENT "Building all test executables"
  )
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
