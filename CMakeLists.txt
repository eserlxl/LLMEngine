# Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of LLMEngine and is licensed under
# the GNU General Public License v3.0 or later.
# See the LICENSE file in the project root for details.

cmake_minimum_required(VERSION 3.16)

# ---- Project -----------------------------------------------------------------
project(LLMEngine
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
option(ENABLE_CLANG_TIDY          "Run clang-tidy if available"                     OFF)

# Mutually exclusive modes sanity check
if(DEBUG_MODE AND PERFORMANCE_BUILD)
  message(FATAL_ERROR "DEBUG_MODE and PERFORMANCE_BUILD cannot both be ON.")
endif()

# ---- Dependencies ------------------------------------------------------------
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(cpr REQUIRED)

# ---- Install dirs and prefix --------------------------------------------------
include(GNUInstallDirs)
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "Install prefix" FORCE)
endif()

# ---- Helper interface targets ------------------------------------------------
# project_options: optimization/sanitizer/defines that apply to all targets
add_library(project_options INTERFACE)

target_include_directories(project_options INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>)

# project_warnings: warning levels per compiler
add_library(project_warnings INTERFACE)

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
  # Prefer hidden visibility by default for smaller/safer libraries
  target_compile_options(project_options INTERFACE -fvisibility=hidden -fvisibility-inlines-hidden)
  if (DEBUG_MODE)
    target_compile_options(project_options INTERFACE -O0 -g -fno-omit-frame-pointer)
    target_compile_definitions(project_options INTERFACE DEBUG)
    if (ENABLE_SANITIZERS)
      target_compile_options(project_options INTERFACE -fsanitize=address,undefined)
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
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
  endif()
endif()

# ---- Helper macro for reusable library alias pattern ------------------------
# Creates a namespaced alias for a library target (e.g., mylib::mylib)
# Usage: create_namespaced_alias(${PROJECT_NAME})
macro(create_namespaced_alias target_name)
  string(TOLOWER "${target_name}" namespace)
  add_library(${namespace}::${namespace} ALIAS ${target_name})
endmacro()

# ---- Library ---------------------------------------------------------------
add_library(${PROJECT_NAME} STATIC
    src/LLMEngine.cpp
    src/LLMOutputProcessor.cpp
    src/Utils.cpp
    src/APIClient.cpp
)

target_link_libraries(${PROJECT_NAME} PUBLIC
  cpr::cpr
  nlohmann_json::nlohmann_json
  project_options
  project_warnings)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

# ---- Install ------------------------------------------------------------------
# Install library
install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR})

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

# (Removed DEFAULT_CONFIG_PATH; the entire config directory is installed via install(DIRECTORY ...))

# Define default reports and prompts path as compile-time constants
set(REPORTS_INSTALL_PATH "${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME_LOWER}/reports")
target_compile_definitions(${PROJECT_NAME} PUBLIC
  DEFAULT_REPORTS_PATH="${REPORTS_INSTALL_PATH}")

set(PROMPTS_INSTALL_PATH "${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME_LOWER}/prompts")
target_compile_definitions(${PROJECT_NAME} PUBLIC
  DEFAULT_PROMPTS_PATH="${PROMPTS_INSTALL_PATH}")

# Provide a namespaced alias for consumers
create_namespaced_alias(${PROJECT_NAME})

# ---- IPO/LTO (only when meaningful) -----------------------------------------
include(CheckIPOSupported)
set(_ipo_allowed OFF)
check_ipo_supported(RESULT _ipo_allowed OUTPUT _ipo_msg)
if (PERFORMANCE_BUILD AND _ipo_allowed)
  set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  message(STATUS "IPO/LTO enabled for ${PROJECT_NAME}")
else()
  message(STATUS "IPO/LTO not enabled: ${_ipo_msg}")
endif()

# ---- Summary -----------------------------------------------------------------
message(STATUS "========== Build Configuration ==========")
message(STATUS "CMAKE_CXX_COMPILER       : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "CMAKE_BUILD_TYPE         : ${CMAKE_BUILD_TYPE}")
message(STATUS "DEBUG_MODE               : ${DEBUG_MODE}")
message(STATUS "PERFORMANCE_BUILD        : ${PERFORMANCE_BUILD}")
message(STATUS "WARNING_MODE             : ${WARNING_MODE}")
message(STATUS "BUILD_TESTING            : ${BUILD_TESTING}")
message(STATUS "ENABLE_EXAMPLES          : ${ENABLE_EXAMPLES}")
message(STATUS "ENABLE_NATIVE_OPTIMIZATION: ${ENABLE_NATIVE_OPTIMIZATION}")
message(STATUS "ENABLE_SANITIZERS        : ${ENABLE_SANITIZERS}")
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