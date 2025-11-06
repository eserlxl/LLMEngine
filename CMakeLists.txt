# Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This CMake file configures the LLMEngine core library build and packaging,
# and is licensed under the GNU General Public License v3.0 or later.
# See the LICENSE file in the project root for details.

cmake_minimum_required(VERSION 3.20)

# Ensure reproducible FetchContent timestamp handling
if (POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

# ---- Project -----------------------------------------------------------------
project(LLMEngine
  VERSION 0.1.0
  DESCRIPTION "A C++ library for interacting with Large Language Models"
  LANGUAGES CXX)

# _GNU_SOURCE will be applied via target_compile_definitions only to targets that need it
# This avoids mutating global flags that affect all dependencies and consumers

# Default build type when not specified (for single-config generators and IDEs)
if (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type")
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# Lowercase project name for filesystem-friendly install directories
string(TOLOWER "${PROJECT_NAME}" PROJECT_NAME_LOWER)

# C++ standard is ensured via target_compile_features on the library target

# Build system quality-of-life
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Avoid global runtime output directory; use per-target properties instead

# ---- Options -----------------------------------------------------------------
option(DEBUG_MODE                 "Enable debug build flags"                       OFF)
option(PERFORMANCE_BUILD          "Enable performance-optimized flags"             OFF)
option(WARNING_MODE               "Enable extra warnings"                          ON)
option(BUILD_TESTING              "Build tests and enable CTest"                   ON)
option(ENABLE_EXAMPLES            "Build example programs"                          ON)
option(ENABLE_NATIVE_OPTIMIZATION "Use -march=native/-mtune=native in performance" OFF)
option(ENABLE_SANITIZERS          "Enable Address/Undefined sanitizers in debug"    OFF)
option(BUILD_SHARED_LIBS          "Build shared instead of static libraries"       OFF)
option(ENABLE_CLANG_TIDY          "Run clang-tidy if available"                     OFF)
option(ENABLE_COVERAGE            "Enable coverage flags in Debug (GCC/Clang)"     OFF)
option(ENABLE_DOXYGEN             "Enable Doxygen documentation target"            OFF)
option(WERROR                     "Treat warnings as errors (for CI builds)"       OFF)
option(LLM_DISABLE_FETCHCONTENT   "Disable FetchContent; require system packages"  OFF)
option(LLMENGINE_ENABLE_VISIBILITY_HIDDEN "Enable -fvisibility=hidden for shared builds" ON)

# Fine-grained toggles (optional; override ENABLE_SANITIZERS when set)
option(LLM_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(LLM_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(LLM_ENABLE_MSAN "Enable MemorySanitizer (Clang-only)" OFF)
option(LLM_ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(LLM_ENABLE_LTO  "Force enable IPO/LTO when supported" OFF)

# Mutually exclusive modes sanity check
if(DEBUG_MODE AND PERFORMANCE_BUILD)
  message(FATAL_ERROR "DEBUG_MODE and PERFORMANCE_BUILD cannot both be ON.")
endif()

# Configure FetchContent base directory to ensure it's set before any FetchContent_Declare calls
# This fixes issues with empty work tree paths in CI environments
if(NOT DEFINED FETCHCONTENT_BASE_DIR)
  set(FETCHCONTENT_BASE_DIR "${CMAKE_BINARY_DIR}/_deps" CACHE PATH "Base directory for FetchContent dependencies")
endif()
# Ensure the directory exists
file(MAKE_DIRECTORY "${FETCHCONTENT_BASE_DIR}")

# ---- Dependencies ------------------------------------------------------------
# Note: OpenSSL is not directly required by this library, but cpr depends on it.
# If cpr is fetched via FetchContent, it will handle its own OpenSSL dependency.
# If cpr is provided by the system, we need to find OpenSSL to satisfy cpr's interface requirements.

# Prefer system-provided nlohmann_json via config; fall back to FetchContent
find_package(nlohmann_json CONFIG QUIET)
set(NLOHMANN_JSON_FETCHED FALSE)
if (NOT nlohmann_json_FOUND)
  if (LLM_DISABLE_FETCHCONTENT)
    message(FATAL_ERROR
      "nlohmann_json not found and LLM_DISABLE_FETCHCONTENT=ON.\n"
      "Please install nlohmann_json (v3.11.3 or compatible) via your system package manager,\n"
      "or set LLM_DISABLE_FETCHCONTENT=OFF to allow automatic fetching.")
  endif()
  include(FetchContent)
  message(STATUS "nlohmann_json not found via package config; fetching v3.11.3 with FetchContent")
  FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    SOURCE_DIR "${FETCHCONTENT_BASE_DIR}/nlohmann_json-src"
    FIND_PACKAGE_ARGS NAMES nlohmann_json
  )
  FetchContent_MakeAvailable(nlohmann_json)
  set(NLOHMANN_JSON_FETCHED TRUE)
  # Some environments expose an un-namespaced target; normalize if needed
  if (TARGET nlohmann_json AND NOT TARGET nlohmann_json::nlohmann_json)
    add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
  endif()
endif()

# Prefer system-provided cpr via config; fall back to FetchContent
find_package(cpr CONFIG QUIET)
set(CPR_FETCHED FALSE)
if (cpr_FOUND)
  # System-provided cpr requires OpenSSL to be found for its interface dependencies
  find_package(OpenSSL REQUIRED)
else()
  if (LLM_DISABLE_FETCHCONTENT)
    message(FATAL_ERROR
      "cpr not found and LLM_DISABLE_FETCHCONTENT=ON.\n"
      "Please install cpr (v1.10.5 or compatible) and OpenSSL via your system package manager,\n"
      "or set LLM_DISABLE_FETCHCONTENT=OFF to allow automatic fetching.")
  endif()
  include(FetchContent)
  
  # Check if source is already available (pre-cloned in CI)
  set(_cpr_source_dir "${FETCHCONTENT_BASE_DIR}/cpr-src")
  # Save current FETCHCONTENT_UPDATES_DISCONNECTED value to restore after CPR setup
  # This prevents the CPR-specific setting from affecting other FetchContent dependencies
  get_property(_saved_fetchcontent_updates_disconnected CACHE FETCHCONTENT_UPDATES_DISCONNECTED PROPERTY VALUE)
  if(EXISTS "${_cpr_source_dir}/CMakeLists.txt")
    message(STATUS "cpr source already available at ${_cpr_source_dir}, using pre-cloned source")
    set(FETCHCONTENT_SOURCE_DIR_CPR "${_cpr_source_dir}" CACHE PATH "Source directory for cpr" FORCE)
    # Temporarily set FETCHCONTENT_UPDATES_DISCONNECTED for CPR only
    set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "Skip updates for FetchContent" FORCE)
  else()
    message(STATUS "cpr not found via package config; fetching v1.10.5 with FetchContent")
  endif()
  
  # Use a released tag for reproducible builds
  FetchContent_Declare(
    cpr
    GIT_REPOSITORY https://github.com/libcpr/cpr.git
    GIT_TAG 1.10.5
    SOURCE_DIR "${_cpr_source_dir}"
    FIND_PACKAGE_ARGS NAMES cpr
  )
  set(CPR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(CPR_BUILD_TESTS_SSL OFF CACHE BOOL "" FORCE)
  set(CPR_ENABLE_SSL ON CACHE BOOL "" FORCE)
  # Use CPR_BUILD_SHARED_LIBS option instead of mutating global BUILD_SHARED_LIBS cache
  # This respects user's choice and avoids breaking multi-config generators
  set(CPR_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(cpr)
  set(CPR_FETCHED TRUE)
  # Restore original FETCHCONTENT_UPDATES_DISCONNECTED value to avoid affecting other FetchContent dependencies
  if(DEFINED _saved_fetchcontent_updates_disconnected)
    set(FETCHCONTENT_UPDATES_DISCONNECTED "${_saved_fetchcontent_updates_disconnected}" CACHE BOOL "Skip updates for FetchContent" FORCE)
  else()
    # If it wasn't set before, remove it from cache to restore default behavior
    unset(FETCHCONTENT_UPDATES_DISCONNECTED CACHE)
  endif()
  # Some environments expose an un-namespaced target; normalize if needed
  if (TARGET cpr AND NOT TARGET cpr::cpr)
    add_library(cpr::cpr ALIAS cpr)
  endif()
endif()

# ---- Install dirs and prefix --------------------------------------------------
include(GNUInstallDirs)

# ---- Helper interface targets ------------------------------------------------
# Introduce INTERFACE target to centralize common compile/link options
add_library(${PROJECT_NAME}_compile_options INTERFACE)
string(TOLOWER "${PROJECT_NAME}" _ns)
add_library(${_ns}::compile_options ALIAS ${PROJECT_NAME}_compile_options)
set_target_properties(${PROJECT_NAME}_compile_options PROPERTIES EXPORT_NAME compile_options)

# C++ standard is set on the library target, not on compile_options
# (compile_options is linked PRIVATE, so it doesn't propagate to consumers)
if (MSVC)
  target_compile_options(${PROJECT_NAME}_compile_options INTERFACE /permissive-)
else()
  # Use generator expressions to scope flags to specific compilers
  # -fstack-protector-strong and -Wformat=2 may not be supported on all compilers (e.g., Apple Clang)
  target_compile_options(${PROJECT_NAME}_compile_options INTERFACE
    $<$<AND:$<COMPILE_LANG_AND_ID:CXX,GNU>,$<NOT:$<PLATFORM_ID:Darwin>>>:-fstack-protector-strong>
    $<$<AND:$<COMPILE_LANG_AND_ID:CXX,Clang>,$<NOT:$<PLATFORM_ID:Darwin>>>:-fstack-protector-strong>)
  if (UNIX AND NOT APPLE)
    target_link_options(${PROJECT_NAME}_compile_options INTERFACE -Wl,-z,relro,-z,now)
  endif()
  if (WARNING_MODE)
    target_compile_options(${PROJECT_NAME}_compile_options INTERFACE
      -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wnon-virtual-dtor
      -Wold-style-cast -Woverloaded-virtual -Wnull-dereference -Wimplicit-fallthrough
      $<$<AND:$<COMPILE_LANG_AND_ID:CXX,GNU>,$<NOT:$<PLATFORM_ID:Darwin>>>:-Wformat=2>
      $<$<AND:$<COMPILE_LANG_AND_ID:CXX,Clang>,$<NOT:$<PLATFORM_ID:Darwin>>>:-Wformat=2>)
    if (WERROR)
      target_compile_options(${PROJECT_NAME}_compile_options INTERFACE -Werror)
    endif()
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_compile_options(${PROJECT_NAME}_compile_options INTERFACE -Wduplicated-cond -Wlogical-op -Wuseless-cast)
    endif()
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      target_compile_options(${PROJECT_NAME}_compile_options INTERFACE -Wextra-semi -Wno-c99-extensions)
    endif()
  endif()
endif()

# Pin MSVC runtime library deterministically
if (MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "MSVC runtime library")
endif()

# (Removed unused helper target flag wiring; flags applied directly to main target below)

# Optional clang-tidy (set only on our targets, not globally, to avoid running on dependencies)
# ccache/sccache detection
if (NOT CMAKE_CXX_COMPILER_LAUNCHER)
  find_program(CCACHE_PROGRAM ccache)
  if (CCACHE_PROGRAM)
    message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
  else()
    find_program(SCCACHE_PROGRAM sccache)
    if (SCCACHE_PROGRAM)
      message(STATUS "Using sccache: ${SCCACHE_PROGRAM}")
      set(CMAKE_CXX_COMPILER_LAUNCHER ${SCCACHE_PROGRAM})
    endif()
  endif()
endif()
# Use CACHE so subdirectories can access it
set(_CLANG_TIDY_CMD "" CACHE STRING "clang-tidy command for our targets")
if (ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY_EXE NAMES clang-tidy)
  if (CLANG_TIDY_EXE)
    set(_CLANG_TIDY_CMD "${CLANG_TIDY_EXE};-warnings-as-errors=*;-header-filter=^(src|examples|test)/.*" CACHE STRING "clang-tidy command for our targets" FORCE)
    message(STATUS "clang-tidy enabled: ${CLANG_TIDY_EXE} (will be applied to LLMEngine targets only)")
  else()
    message(WARNING "ENABLE_CLANG_TIDY is ON but clang-tidy not found")
    set(_CLANG_TIDY_CMD "" CACHE STRING "clang-tidy command for our targets" FORCE)
  endif()
endif()

# Helper function to apply clang-tidy to a target (for use in subdirectories)
function(apply_clang_tidy_to_target target_name)
  if (_CLANG_TIDY_CMD)
    set_target_properties(${target_name} PROPERTIES CXX_CLANG_TIDY "${_CLANG_TIDY_CMD}")
  endif()
endfunction()

# ---- Helper macros for reusable patterns ------------------------------------
# Creates a namespaced alias for a library target (e.g., llmengine::LLMEngine)
# Usage: create_namespaced_alias(${PROJECT_NAME})
# Preserves original target name after :: for clarity
macro(create_namespaced_alias target_name)
  string(TOLOWER "${target_name}" namespace)
  add_library(${namespace}::${target_name} ALIAS ${target_name})
endmacro()

# Sets the exported imported target name to preserve original target name
# Usage: set_export_name(${PROJECT_NAME})
# Keeps original target name for clarity (e.g., llmengine::LLMEngine)
macro(set_export_name target_name)
  set_target_properties(${target_name} PROPERTIES EXPORT_NAME ${target_name})
endmacro()

# ---- Library ---------------------------------------------------------------
add_library(${PROJECT_NAME}
    src/LLMEngine.cpp
    src/Utils.cpp
    src/APIClient.cpp
    src/OpenAICompatibleClient.cpp
    src/QwenClient.cpp
    src/OpenAIClient.cpp
    src/AnthropicClient.cpp
    src/OllamaClient.cpp
    src/GeminiClient.cpp
    src/DebugArtifacts.cpp
    src/DebugArtifactManager.cpp
    src/RequestLogger.cpp
    src/ResponseParser.cpp
    src/ParameterMerger.cpp
    src/RequestContextBuilder.cpp
    src/ResponseHandler.cpp
    src/ProviderBootstrap.cpp
    src/TempDirectoryService.cpp
)

# Versioning for shared libraries
set_target_properties(${PROJECT_NAME} PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR})

# Apply clang-tidy only to our target (not dependencies)
if (_CLANG_TIDY_CMD)
  set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_CLANG_TIDY "${_CLANG_TIDY_CMD}")
endif()

# Public dependencies (exported to consumers)
# For FetchContent dependencies, link PRIVATE since consumers will provide their own
# via find_dependency() in Config.cmake. For system packages, link PUBLIC so they're exported.
if (NLOHMANN_JSON_FETCHED)
  # Link PRIVATE - consumers will provide their own via find_dependency()
  target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)
else()
  # System package - link PUBLIC so it's exported normally
  target_link_libraries(${PROJECT_NAME} PUBLIC nlohmann_json::nlohmann_json)
endif()

if (CPR_FETCHED)
  # Link PRIVATE - consumers will provide their own via find_dependency()
  target_link_libraries(${PROJECT_NAME} PRIVATE cpr::cpr)
  # Treat CPR headers as system headers to suppress warnings from external dependency
  get_target_property(CPR_INCLUDE_DIRS cpr::cpr INTERFACE_INCLUDE_DIRECTORIES)
  if (CPR_INCLUDE_DIRS)
    target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${CPR_INCLUDE_DIRS})
  endif()
else()
  # System package - link PUBLIC so it's exported normally
  target_link_libraries(${PROJECT_NAME} PUBLIC cpr::cpr)
endif()

# Propagate common compile options to library only (not to consumers)
# Keep compile options PRIVATE to avoid leaking build policy decisions to consumers
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_compile_options)

# Include directories (needed by consumers)
# Use original include/ directory structure - no need to copy headers
# Use single canonical include layout: include/ for compatibility headers, include/${PROJECT_NAME}/ for namespaced headers
target_include_directories(${PROJECT_NAME} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated_include>
  $<INSTALL_INTERFACE:include>)

# Apply _GNU_SOURCE only to our target if needed (for POSIX extensions)
# This is applied via target_compile_definitions instead of global flags
if(UNIX AND NOT APPLE)
  target_compile_definitions(${PROJECT_NAME} PRIVATE _GNU_SOURCE)
endif()

# Build-time configuration is applied directly to the target (avoid exporting helpers)
# Note: Hardening and warning flags are already applied via ${PROJECT_NAME}_compile_options
if (MSVC)
  if (DEBUG_MODE)
    target_compile_options(${PROJECT_NAME} PRIVATE /Od /Z7)
    target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG)
  elseif (PERFORMANCE_BUILD)
    target_compile_options(${PROJECT_NAME} PRIVATE /O2)
    target_compile_definitions(${PROJECT_NAME} PRIVATE NDEBUG)
  else()
    target_compile_options(${PROJECT_NAME} PRIVATE /O2 /Z7)
  endif()
else()
  # Visibility flags are disabled until an export header is introduced for shared builds
  # Modes
  if (BUILD_SHARED_LIBS AND LLMENGINE_ENABLE_VISIBILITY_HIDDEN)
    # Apply default hidden visibility for shared libs; exported symbols annotated by generated header
    target_compile_options(${PROJECT_NAME} PRIVATE -fvisibility=hidden -fvisibility-inlines-hidden)
  endif()
  if (DEBUG_MODE)
    target_compile_options(${PROJECT_NAME} PRIVATE -O0 -g -fno-omit-frame-pointer)
    target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_compile_definitions(${PROJECT_NAME} PRIVATE _GLIBCXX_ASSERTIONS)
    endif()
    # Sanitizers
    set(_san_list "")
    if (LLM_ENABLE_ASAN OR ENABLE_SANITIZERS)
      list(APPEND _san_list "address")
    endif()
    if (LLM_ENABLE_UBSAN OR ENABLE_SANITIZERS)
      list(APPEND _san_list "undefined")
    endif()
    if (LLM_ENABLE_MSAN)
      list(APPEND _san_list "memory")
    endif()
    if (LLM_ENABLE_TSAN)
      list(APPEND _san_list "thread")
    endif()
    if (_san_list)
      string(REPLACE ";" "," _san_flags "${_san_list}")
      # Only apply sanitizer flags on compilers that support them (GCC/Clang, not MSVC)
      target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-fsanitize=${_san_flags}>
        $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-fno-omit-frame-pointer>)
      target_link_options(${PROJECT_NAME} PRIVATE
        $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-fsanitize=${_san_flags}>)
    endif()
  elseif (PERFORMANCE_BUILD)
    target_compile_options(${PROJECT_NAME} PRIVATE -O3)
    if (ENABLE_NATIVE_OPTIMIZATION)
      # Apply native tuning safely based on compiler and platform; disable when cross-compiling
      target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<AND:$<COMPILE_LANG_AND_ID:CXX,GNU>,$<NOT:$<BOOL:CMAKE_CROSSCOMPILING>>>:-march=native>
        $<$<AND:$<COMPILE_LANG_AND_ID:CXX,GNU>,$<NOT:$<BOOL:CMAKE_CROSSCOMPILING>>>:-mtune=native>
        $<$<AND:$<COMPILE_LANG_AND_ID:CXX,Clang>,$<NOT:$<PLATFORM_ID:Darwin>>,$<NOT:$<BOOL:CMAKE_CROSSCOMPILING>>>:-march=native>
      )
    endif()
    target_compile_definitions(${PROJECT_NAME} PRIVATE NDEBUG _FORTIFY_SOURCE=2)
  else()
    target_compile_options(${PROJECT_NAME} PRIVATE -O2 -g -fno-omit-frame-pointer)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _FORTIFY_SOURCE=2)
  endif()
endif()

# ---- Coverage (Debug only) ----------------------------------------------------
if (ENABLE_COVERAGE AND NOT MSVC)
  if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
      if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(${PROJECT_NAME} PRIVATE -fprofile-instr-generate -fcoverage-mapping)
        target_link_options(${PROJECT_NAME} PRIVATE -fprofile-instr-generate -fcoverage-mapping)
      else()
        target_compile_options(${PROJECT_NAME} PRIVATE --coverage -O0 -g)
        target_link_options(${PROJECT_NAME} PRIVATE --coverage)
      endif()
      message(STATUS "Coverage flags enabled for ${PROJECT_NAME}")
    endif()
  endif()
endif()

# Only generate the export header that requires build-time generation
# Rely on target_include_directories to use the original include/ directory
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated_include/LLMEngine")

# ---- Export header (visibility) ----------------------------------------------
include(GenerateExportHeader)
generate_export_header(${PROJECT_NAME}
  EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/generated_include/LLMEngine/LLMEngineExport.hpp")

# Note: Warnings and hardening flags are already applied via ${PROJECT_NAME}_compile_options interface target
# This avoids duplication and ensures a single source of truth for compile options

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

# ---- Install ------------------------------------------------------------------
# Install library (handles both static and shared) and export targets.
# We always export our own targets; consumers are responsible for providing
# third-party dependencies (cpr, nlohmann_json) discovered via find_dependency().
# Note: FetchContent dependencies are installed into the export set to make them
# exportable and prevent CMake validation errors, but consumers should still use
# find_dependency() to get them via Config.cmake.

# Install only our own targets into export set
# External dependencies (nlohmann_json, cpr) are NOT exported to avoid referencing
# build-tree paths (_deps/...-src) that disappear after build directory cleanup.
# Consumers must use find_dependency() in Config.cmake to discover dependencies
# from the system or their own fetch logic.
set(_targets_to_install ${PROJECT_NAME} ${PROJECT_NAME}_compile_options)

install(TARGETS ${_targets_to_install}
  EXPORT ${PROJECT_NAME}Targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Note: INTERFACE_LINK_LIBRARIES restoration happens after install(EXPORT)
# to ensure export files are generated with filtered dependencies

# Install public headers
# Namespaced headers go to include/LLMEngine/
install(DIRECTORY include/LLMEngine/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}
    FILES_MATCHING PATTERN "*.hpp")
# HttpStatus.hpp is included in the directory install above
# Compatibility headers go to include root (so both <LLMEngine.hpp> and <LLMEngine/LLMEngine.hpp> work)
install(FILES
    include/LLMEngine.hpp
    include/APIClient.hpp
    include/Utils.hpp
    include/Logger.hpp
    include/RequestLogger.hpp
    include/DebugArtifacts.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/generated_include/LLMEngine/LLMEngineExport.hpp"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})

# Install config and scripts (if these directories exist)
install(DIRECTORY config/  DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME_LOWER}/config OPTIONAL)
install(DIRECTORY scripts/ DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME_LOWER}/scripts OPTIONAL)
install(DIRECTORY prompts/ DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME_LOWER}/prompts OPTIONAL)
install(DIRECTORY reports/ DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME_LOWER}/reports OPTIONAL)

# Note: DEFAULT_REPORTS_PATH and DEFAULT_PROMPTS_PATH should be computed at runtime
# to avoid hardcoding absolute paths from the build machine. Consumers should use
# runtime helpers or configuration files to determine these paths relative to
# installation location or current working directory.

# Provide a namespaced alias and set exported imported name (preserve original target name)
create_namespaced_alias(${PROJECT_NAME})
set_export_name(${PROJECT_NAME})

# ---- Package config (find_package support) -----------------------------------
include(CMakePackageConfigHelpers)

# Generate version file
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

# Always generate config file (even when dependencies are fetched)
# This allows find_package() to work in build-tree scenarios
configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Config.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")

# Export targets to the build tree so examples can use find_package() without install
# Note: INTERFACE_LINK_LIBRARIES filtering already happened before install(TARGETS ... EXPORT)
export(EXPORT ${PROJECT_NAME}Targets
  NAMESPACE ${PROJECT_NAME_LOWER}::
  FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Targets.cmake")

# Always install config files (even when dependencies are fetched)
# The Config.cmake will document dependency requirements
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

# Install export set for our own targets only
# External dependencies are not exported; consumers use find_dependency() in Config.cmake
install(EXPORT ${PROJECT_NAME}Targets
  NAMESPACE ${PROJECT_NAME_LOWER}::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

# ---- pkg-config ---------------------------------------------------------------
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/llmengine.pc.in"
  "${CMAKE_CURRENT_BINARY_DIR}/llmengine.pc"
  @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/llmengine.pc"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

# ---- IPO/LTO (only when meaningful) -----------------------------------------
include(CheckIPOSupported)
set(_ipo_allowed OFF)
check_ipo_supported(RESULT _ipo_allowed OUTPUT _ipo_msg)
if ((PERFORMANCE_BUILD OR LLM_ENABLE_LTO) AND _ipo_allowed)
  set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  message(STATUS "IPO/LTO enabled for ${PROJECT_NAME}")
elseif(PERFORMANCE_BUILD OR LLM_ENABLE_LTO)
  message(STATUS "IPO/LTO not enabled: ${_ipo_msg}")
endif()

# ---- Tests / Examples --------------------------------------------------------
if (BUILD_TESTING)
  enable_testing()
  if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt")
    add_subdirectory(test EXCLUDE_FROM_ALL)
  endif()
endif()

# ---- Packaging (CPack) -------------------------------------------------------
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VENDOR "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_GENERATOR "TGZ")
include(CPack)

if (ENABLE_EXAMPLES)
  if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/CMakeLists.txt")
    add_subdirectory(examples EXCLUDE_FROM_ALL)
  endif()
endif()

# ---- Summary -----------------------------------------------------------------
# Verbose summary output (can be disabled for CI-friendly builds)
option(LLM_VERBOSE_CONFIG_SUMMARY "Print detailed build configuration summary" ON)
if (LLM_VERBOSE_CONFIG_SUMMARY)
    message(STATUS "========== Build Configuration ==========")
    message(STATUS "CMAKE_CXX_COMPILER       : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
    message(STATUS "CMAKE_BUILD_TYPE         : ${CMAKE_BUILD_TYPE}")
    message(STATUS "DEBUG_MODE               : ${DEBUG_MODE}")
    message(STATUS "PERFORMANCE_BUILD        : ${PERFORMANCE_BUILD}")
    message(STATUS "WARNING_MODE             : ${WARNING_MODE}")
    message(STATUS "BUILD_SHARED_LIBS        : ${BUILD_SHARED_LIBS}")
    message(STATUS "BUILD_TESTING            : ${BUILD_TESTING}")
    message(STATUS "ENABLE_EXAMPLES          : ${ENABLE_EXAMPLES}")
    message(STATUS "ENABLE_NATIVE_OPTIMIZATION: ${ENABLE_NATIVE_OPTIMIZATION}")
    message(STATUS "ENABLE_SANITIZERS        : ${ENABLE_SANITIZERS}")
    message(STATUS "ENABLE_CLANG_TIDY        : ${ENABLE_CLANG_TIDY}")
    get_target_property(_ipo ${PROJECT_NAME} INTERPROCEDURAL_OPTIMIZATION)
    message(STATUS "IPO/LTO (${PROJECT_NAME})      : ${_ipo}")
    message(STATUS "Runtime output directory : ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    message(STATUS "---------- Install Directories -----------")
    message(STATUS "CMAKE_INSTALL_PREFIX     : ${CMAKE_INSTALL_PREFIX}")
    message(STATUS "BIN dir                  : ${CMAKE_INSTALL_BINDIR} (full: ${CMAKE_INSTALL_FULL_BINDIR})")
    message(STATUS "LIB dir                  : ${CMAKE_INSTALL_LIBDIR} (full: ${CMAKE_INSTALL_FULL_LIBDIR})")
    message(STATUS "INCLUDE dir              : ${CMAKE_INSTALL_INCLUDEDIR} (full: ${CMAKE_INSTALL_FULL_INCLUDEDIR})")
    message(STATUS "DATA root dir            : ${CMAKE_INSTALL_DATAROOTDIR} (full: ${CMAKE_INSTALL_FULL_DATAROOTDIR})")
    message(STATUS "DATA dir                 : ${CMAKE_INSTALL_DATADIR} (full: ${CMAKE_INSTALL_FULL_DATADIR})")
    message(STATUS "LIBEXEC dir              : ${CMAKE_INSTALL_LIBEXECDIR} (full: ${CMAKE_INSTALL_FULL_LIBEXECDIR})")
    message(STATUS "SYSCONF dir              : ${CMAKE_INSTALL_SYSCONFDIR} (full: ${CMAKE_INSTALL_FULL_SYSCONFDIR})")
    message(STATUS "========================================")
else()
    message(STATUS "${PROJECT_NAME} ${PROJECT_VERSION}: ${CMAKE_BUILD_TYPE} build (${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION})")
endif()

# ---- Doxygen (optional) -------------------------------------------------------
if (ENABLE_DOXYGEN)
  find_package(Doxygen QUIET)
  if (DOXYGEN_FOUND)
    add_custom_target(docs
      COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMENT "Generating API documentation with Doxygen"
      VERBATIM)
  else()
    message(STATUS "Doxygen not found; docs target disabled")
  endif()
endif()