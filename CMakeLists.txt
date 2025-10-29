# Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of LLMEngine and is licensed under
# the GNU General Public License v3.0 or later.
# See the LICENSE file in the project root for details.

cmake_minimum_required(VERSION 3.16)

# ---- Project -----------------------------------------------------------------
project(LLMEngine
  VERSION 0.1.0
  DESCRIPTION "A C++ library for interacting with Large Language Models"
  LANGUAGES CXX)

# Lowercase project name for filesystem-friendly install directories
string(TOLOWER "${PROJECT_NAME}" PROJECT_NAME_LOWER)

# C++ standard (no compiler extensions)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build system quality-of-life
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Put built binaries under <build>/bin (and per-config subdirs for multi-config generators)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
foreach(cfg IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
  string(TOUPPER "${cfg}" CFG_UP)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_UP} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${cfg}")
endforeach()

# ---- Options -----------------------------------------------------------------
option(DEBUG_MODE                 "Enable debug build flags"                       OFF)
option(PERFORMANCE_BUILD          "Enable performance-optimized flags"             ON)
option(WARNING_MODE               "Enable extra warnings"                          ON)
option(BUILD_TESTING              "Build tests and enable CTest"                   ON)
option(ENABLE_EXAMPLES            "Build example programs"                          ON)
option(ENABLE_NATIVE_OPTIMIZATION "Use -march=native/-mtune=native in performance" OFF)
option(ENABLE_SANITIZERS          "Enable Address/Undefined sanitizers in debug"    OFF)
option(BUILD_SHARED_LIBS          "Build shared instead of static libraries"       OFF)
option(ENABLE_CLANG_TIDY          "Run clang-tidy if available"                     OFF)

# Mutually exclusive modes sanity check
if(DEBUG_MODE AND PERFORMANCE_BUILD)
  message(FATAL_ERROR "DEBUG_MODE and PERFORMANCE_BUILD cannot both be ON.")
endif()

# ---- Dependencies ------------------------------------------------------------
# OpenSSL must be found for cpr's CMake config (cpr links it transitively)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(cpr REQUIRED)

# ---- Install dirs and prefix --------------------------------------------------
include(GNUInstallDirs)
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "Install prefix" FORCE)
endif()

# ---- Helper interface targets ------------------------------------------------
# project_options: optimization/sanitizer/defines that apply to all targets (PRIVATE)
add_library(project_options INTERFACE)

# project_warnings: warning levels per compiler (PRIVATE)
add_library(project_warnings INTERFACE)

# Only add warnings if WARNING_MODE is enabled
if (WARNING_MODE)
  # Detect compiler family
  if (MSVC)
    # /O flags handled via mode below; keep warnings modern
    target_compile_options(project_warnings INTERFACE
      /W4
      /permissive-)
  else()
    # Reasonably strict but practical defaults
    target_compile_options(project_warnings INTERFACE
      -Wall
      -Wextra
      -Wpedantic
      -Wconversion
      -Wshadow
      -Wnon-virtual-dtor
      -Wold-style-cast
      -Woverloaded-virtual
      -Wnull-dereference
      -Wformat=2
      -Wimplicit-fallthrough)
    # GCC-only extra warnings
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_compile_options(project_warnings INTERFACE
        -Wduplicated-cond
        -Wlogical-op
        -Wuseless-cast)
    endif()
    # Clang-only tweaks
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      target_compile_options(project_warnings INTERFACE
        -Wextra-semi
        -Wno-c99-extensions)
    endif()
  endif()
endif()

# Pin MSVC runtime library deterministically
if (MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "MSVC runtime library")
endif()

# Optimization / Debug / Sanitizer flags
if (MSVC)
  if (DEBUG_MODE)
    target_compile_options(project_options INTERFACE /Od /Z7)
    target_compile_definitions(project_options INTERFACE DEBUG)
  elseif (PERFORMANCE_BUILD)
    target_compile_options(project_options INTERFACE /O2)
    target_compile_definitions(project_options INTERFACE NDEBUG)
  else()
    target_compile_options(project_options INTERFACE /O2 /Z7)
  endif()
else()
  # Prefer hidden visibility for shared builds only (to avoid empty exports)
  if (BUILD_SHARED_LIBS)
    target_compile_options(project_options INTERFACE -fvisibility=hidden -fvisibility-inlines-hidden)
  endif()
  
  # Hardening: stack protector (works with _FORTIFY_SOURCE=2)
  target_compile_options(project_options INTERFACE -fstack-protector-strong)
  target_link_options(project_options INTERFACE -Wl,-z,relro,-z,now)
  
  if (DEBUG_MODE)
    target_compile_options(project_options INTERFACE -O0 -g -fno-omit-frame-pointer)
    target_compile_definitions(project_options INTERFACE DEBUG)
    # Debug niceties: libstdc++ bounds checking
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_compile_definitions(project_options INTERFACE _GLIBCXX_ASSERTIONS)
    endif()
    if (ENABLE_SANITIZERS)
      target_compile_options(project_options INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
      target_link_options(project_options INTERFACE -fsanitize=address,undefined)
    endif()
  elseif (PERFORMANCE_BUILD)
    target_compile_options(project_options INTERFACE -O3)
    if (ENABLE_NATIVE_OPTIMIZATION)
      target_compile_options(project_options INTERFACE -march=native -mtune=native)
    endif()
    target_compile_definitions(project_options INTERFACE NDEBUG _FORTIFY_SOURCE=2)
  else()
    # Reasonable default when neither mode is explicitly chosen
    target_compile_options(project_options INTERFACE -O2 -g -fno-omit-frame-pointer)
    target_compile_definitions(project_options INTERFACE _FORTIFY_SOURCE=2)
  endif()
endif()

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

# ---- Helper macros for reusable patterns ------------------------------------
# Creates a namespaced alias for a library target (e.g., mylib::mylib)
# Usage: create_namespaced_alias(${PROJECT_NAME})
macro(create_namespaced_alias target_name)
  string(TOLOWER "${target_name}" namespace)
  add_library(${namespace}::${namespace} ALIAS ${target_name})
endmacro()

# Sets the exported imported target name to lowercase for consumers
# Usage: set_lowercase_export_name(${PROJECT_NAME})
macro(set_lowercase_export_name target_name)
  string(TOLOWER "${target_name}" _lower)
  set_target_properties(${target_name} PROPERTIES EXPORT_NAME ${_lower})
endmacro()

# ---- Library ---------------------------------------------------------------
add_library(${PROJECT_NAME}
    src/LLMEngine.cpp
    src/LLMOutputProcessor.cpp
    src/Utils.cpp
    src/APIClient.cpp
)

# Versioning for shared libraries
set_target_properties(${PROJECT_NAME} PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR})

# Public dependencies (exported to consumers)
target_link_libraries(${PROJECT_NAME} PUBLIC
  cpr::cpr
  nlohmann_json::nlohmann_json)

# Include directories (needed by consumers)
target_include_directories(${PROJECT_NAME} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include/${PROJECT_NAME}>)

# Build-time configuration is applied directly to the target (avoid exporting helpers)
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
  # Visibility only for shared builds
  if (BUILD_SHARED_LIBS)
    target_compile_options(${PROJECT_NAME} PRIVATE -fvisibility=hidden -fvisibility-inlines-hidden)
  endif()
  # Hardening
  target_compile_options(${PROJECT_NAME} PRIVATE -fstack-protector-strong)
  target_link_options(${PROJECT_NAME} PRIVATE -Wl,-z,relro,-z,now)
  # Modes
  if (DEBUG_MODE)
    target_compile_options(${PROJECT_NAME} PRIVATE -O0 -g -fno-omit-frame-pointer)
    target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_compile_definitions(${PROJECT_NAME} PRIVATE _GLIBCXX_ASSERTIONS)
    endif()
    if (ENABLE_SANITIZERS)
      target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
      target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address,undefined)
    endif()
  elseif (PERFORMANCE_BUILD)
    target_compile_options(${PROJECT_NAME} PRIVATE -O3)
    if (ENABLE_NATIVE_OPTIMIZATION)
      target_compile_options(${PROJECT_NAME} PRIVATE -march=native -mtune=native)
    endif()
    target_compile_definitions(${PROJECT_NAME} PRIVATE NDEBUG _FORTIFY_SOURCE=2)
  else()
    target_compile_options(${PROJECT_NAME} PRIVATE -O2 -g -fno-omit-frame-pointer)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _FORTIFY_SOURCE=2)
  endif()
endif()

# Warnings configuration (build-time only)
if (WARNING_MODE)
  if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive-)
  else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual -Wnull-dereference -Wformat=2 -Wimplicit-fallthrough)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_compile_options(${PROJECT_NAME} PRIVATE -Wduplicated-cond -Wlogical-op -Wuseless-cast)
    endif()
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      target_compile_options(${PROJECT_NAME} PRIVATE -Wextra-semi -Wno-c99-extensions)
    endif()
  endif()
endif()

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

# ---- Install ------------------------------------------------------------------
# Install library (handles both static and shared) and export targets
install(TARGETS ${PROJECT_NAME}
  EXPORT ${PROJECT_NAME}Targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Install public headers
install(FILES
    src/LLMEngine.hpp
    src/LLMOutputProcessor.hpp
    src/Utils.hpp
    src/APIClient.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})

# Install config and scripts (if these directories exist)
install(DIRECTORY config/  DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME_LOWER}/config OPTIONAL)
install(DIRECTORY scripts/ DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME_LOWER}/scripts OPTIONAL)
install(DIRECTORY prompts/ DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME_LOWER}/prompts OPTIONAL)
install(DIRECTORY reports/ DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME_LOWER}/reports OPTIONAL)

# Define default reports and prompts path as compile-time constants
set(REPORTS_INSTALL_PATH "${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME_LOWER}/reports")
target_compile_definitions(${PROJECT_NAME} PUBLIC
  DEFAULT_REPORTS_PATH="${REPORTS_INSTALL_PATH}")

set(PROMPTS_INSTALL_PATH "${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME_LOWER}/prompts")
target_compile_definitions(${PROJECT_NAME} PUBLIC
  DEFAULT_PROMPTS_PATH="${PROMPTS_INSTALL_PATH}")

# Provide a namespaced alias and set exported imported name (lowercase)
create_namespaced_alias(${PROJECT_NAME})
set_lowercase_export_name(${PROJECT_NAME})

# ---- Package config (find_package support) -----------------------------------
include(CMakePackageConfigHelpers)

# Generate version file
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

# Generate config file from template
configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Config.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")

install(EXPORT ${PROJECT_NAME}Targets
  NAMESPACE llmengine::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

# Install config files
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

# ---- IPO/LTO (only when meaningful) -----------------------------------------
include(CheckIPOSupported)
set(_ipo_allowed OFF)
check_ipo_supported(RESULT _ipo_allowed OUTPUT _ipo_msg)
if (PERFORMANCE_BUILD AND _ipo_allowed)
  set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  message(STATUS "IPO/LTO enabled for ${PROJECT_NAME}")
elseif(PERFORMANCE_BUILD)
  message(STATUS "IPO/LTO not enabled: ${_ipo_msg}")
endif()

# ---- Tests / Examples --------------------------------------------------------
if (BUILD_TESTING)
  enable_testing()
  if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt")
    add_subdirectory(test EXCLUDE_FROM_ALL)
  endif()
endif()

if (ENABLE_EXAMPLES)
  if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/CMakeLists.txt")
    add_subdirectory(examples EXCLUDE_FROM_ALL)
  endif()
endif()

# ---- Summary -----------------------------------------------------------------
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