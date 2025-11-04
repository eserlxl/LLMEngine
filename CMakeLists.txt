# Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This CMake file configures the LLMEngine core library build and packaging,
# and is licensed under the GNU General Public License v3.0 or later.
# See the LICENSE file in the project root for details.

cmake_minimum_required(VERSION 3.20)

# ---- Project -----------------------------------------------------------------
project(LLMEngine
  VERSION 0.1.0
  DESCRIPTION "A C++ library for interacting with Large Language Models"
  LANGUAGES CXX)

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
option(ENABLE_COVERAGE            "Enable coverage flags in Debug (GCC/Clang)"     OFF)
option(ENABLE_DOXYGEN             "Enable Doxygen documentation target"            ON)
option(LLM_DISABLE_FETCHCONTENT   "Disable FetchContent; require system packages"  OFF)

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

# ---- Dependencies ------------------------------------------------------------
# cpr's CMake config may reference OpenSSL targets; ensure they're available locally
find_package(OpenSSL REQUIRED)

# Prefer system-provided nlohmann_json via config; fall back to FetchContent
find_package(nlohmann_json CONFIG QUIET)
set(NLOHMANN_JSON_FETCHED FALSE)
if (NOT nlohmann_json_FOUND)
  if (LLM_DISABLE_FETCHCONTENT)
    message(FATAL_ERROR "nlohmann_json not found and LLM_DISABLE_FETCHCONTENT=ON")
  endif()
  include(FetchContent)
  message(STATUS "nlohmann_json not found via package config; fetching with FetchContent")
  FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
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
if (NOT cpr_FOUND)
  if (LLM_DISABLE_FETCHCONTENT)
    message(FATAL_ERROR "cpr not found and LLM_DISABLE_FETCHCONTENT=ON")
  endif()
  include(FetchContent)
  message(STATUS "cpr not found via package config; fetching with FetchContent")
  # Use a released tag for reproducible builds
  FetchContent_Declare(
    cpr
    GIT_REPOSITORY https://github.com/libcpr/cpr.git
    GIT_TAG 1.10.5
    FIND_PACKAGE_ARGS NAMES cpr
  )
  set(CPR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(CPR_BUILD_TESTS_SSL OFF CACHE BOOL "" FORCE)
  set(CPR_ENABLE_SSL ON CACHE BOOL "" FORCE)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  # Ensure _GNU_SOURCE is defined for curl's build (needed for memrchr and other GNU extensions)
  if(UNIX AND NOT APPLE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE")
  endif()
  FetchContent_MakeAvailable(cpr)
  set(CPR_FETCHED TRUE)
  # Some environments expose an un-namespaced target; normalize if needed
  if (TARGET cpr AND NOT TARGET cpr::cpr)
    add_library(cpr::cpr ALIAS cpr)
  endif()
endif()

# ---- Install dirs and prefix --------------------------------------------------
include(GNUInstallDirs)

# Track if dependencies need to be added to the export set (when fetched via FetchContent)
# This is used later when installing targets to satisfy CMake's export validation
set(NLOHMANN_JSON_NEEDS_EXPORT ${NLOHMANN_JSON_FETCHED})
set(CPR_NEEDS_EXPORT ${CPR_FETCHED})

# ---- Helper interface targets ------------------------------------------------
# Introduce INTERFACE target to centralize common compile/link options
add_library(${PROJECT_NAME}_compile_options INTERFACE)
string(TOLOWER "${PROJECT_NAME}" _ns)
add_library(${_ns}::compile_options ALIAS ${PROJECT_NAME}_compile_options)
set_target_properties(${PROJECT_NAME}_compile_options PROPERTIES EXPORT_NAME compile_options)

target_compile_features(${PROJECT_NAME}_compile_options INTERFACE cxx_std_20)
if (MSVC)
  target_compile_options(${PROJECT_NAME}_compile_options INTERFACE /permissive-)
else()
  target_compile_options(${PROJECT_NAME}_compile_options INTERFACE -fstack-protector-strong)
  target_link_options(${PROJECT_NAME}_compile_options INTERFACE -Wl,-z,relro,-z,now)
  if (WARNING_MODE)
    target_compile_options(${PROJECT_NAME}_compile_options INTERFACE
      -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wnon-virtual-dtor
      -Wold-style-cast -Woverloaded-virtual -Wnull-dereference -Wformat=2 -Wimplicit-fallthrough)
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
    src/DebugArtifacts.cpp
    src/RequestLogger.cpp
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
target_link_libraries(${PROJECT_NAME} PUBLIC
  nlohmann_json::nlohmann_json)
# Private dependencies (implementation only, not exported)
target_link_libraries(${PROJECT_NAME} PRIVATE
  cpr::cpr)

# Propagate common compile options to consumers
target_link_libraries(${PROJECT_NAME} PUBLIC ${PROJECT_NAME}_compile_options)

# Include directories (needed by consumers)
target_include_directories(${PROJECT_NAME} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated_include>
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
  # Visibility flags are disabled until an export header is introduced for shared builds
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
      target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=${_san_flags} -fno-omit-frame-pointer)
      target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=${_san_flags})
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

# Provide build-time "LLMEngine/" include prefix by copying headers into a generated include tree
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated_include/LLMEngine")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/include/LLMEngine/LLMEngine.hpp" "${CMAKE_CURRENT_BINARY_DIR}/generated_include/LLMEngine/LLMEngine.hpp" COPYONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/include/LLMEngine/APIClient.hpp" "${CMAKE_CURRENT_BINARY_DIR}/generated_include/LLMEngine/APIClient.hpp" COPYONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/include/LLMEngine/LLMOutputProcessor.hpp" "${CMAKE_CURRENT_BINARY_DIR}/generated_include/LLMEngine/LLMOutputProcessor.hpp" COPYONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/include/LLMEngine/Utils.hpp" "${CMAKE_CURRENT_BINARY_DIR}/generated_include/LLMEngine/Utils.hpp" COPYONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/include/LLMEngine/Logger.hpp" "${CMAKE_CURRENT_BINARY_DIR}/generated_include/LLMEngine/Logger.hpp" COPYONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/include/LLMEngine/DebugArtifacts.hpp" "${CMAKE_CURRENT_BINARY_DIR}/generated_include/LLMEngine/DebugArtifacts.hpp" COPYONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/include/LLMEngine/RequestLogger.hpp" "${CMAKE_CURRENT_BINARY_DIR}/generated_include/LLMEngine/RequestLogger.hpp" COPYONLY)

# ---- Export header (visibility) ----------------------------------------------
include(GenerateExportHeader)
generate_export_header(${PROJECT_NAME}
  EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/generated_include/LLMEngine/LLMEngineExport.hpp")

# Warnings configuration (build-time only)
if (WARNING_MODE)
  if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive-)
  else()
    target_compile_options(${PROJECT_NAME} PRIVATE
      -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wnon-virtual-dtor
      -Wold-style-cast -Woverloaded-virtual -Wnull-dereference -Wformat=2 -Wimplicit-fallthrough)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_compile_options(${PROJECT_NAME} PRIVATE -Wduplicated-cond -Wlogical-op -Wuseless-cast)
    endif()
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      target_compile_options(${PROJECT_NAME} PRIVATE -Wextra-semi -Wno-c99-extensions)
    endif()
  endif()
endif()

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

# ---- Install ------------------------------------------------------------------
# Install library (handles both static and shared) and export targets
# If dependencies were fetched via FetchContent, include them in the same export set
# to satisfy CMake's export validation (required for PUBLIC dependencies)
set(_targets_to_install ${PROJECT_NAME})
if (NLOHMANN_JSON_NEEDS_EXPORT)
  if (TARGET nlohmann_json)
    list(APPEND _targets_to_install nlohmann_json)
  elseif (TARGET nlohmann_json::nlohmann_json)
    list(APPEND _targets_to_install nlohmann_json::nlohmann_json)
  endif()
endif()
if (CPR_NEEDS_EXPORT AND TARGET cpr)
  list(APPEND _targets_to_install cpr)
endif()
install(TARGETS ${_targets_to_install} ${PROJECT_NAME}_compile_options
  EXPORT ${PROJECT_NAME}Targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Install public headers
install(DIRECTORY include/LLMEngine/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}
    FILES_MATCHING PATTERN "*.hpp")
install(FILES
    include/LLMEngine.hpp
    include/APIClient.hpp
    include/Utils.hpp
    include/Logger.hpp
    include/RequestLogger.hpp
    include/LLMOutputProcessor.hpp
    include/DebugArtifacts.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/generated_include/LLMEngine/LLMEngineExport.hpp"
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

# Export targets to the build tree so examples can use find_package() without install
export(EXPORT ${PROJECT_NAME}Targets
  NAMESPACE ${PROJECT_NAME_LOWER}::
  FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Targets.cmake")

install(EXPORT ${PROJECT_NAME}Targets
  NAMESPACE ${PROJECT_NAME_LOWER}::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

# Install config files
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
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