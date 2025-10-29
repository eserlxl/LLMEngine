# Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of LLMEngine and is licensed under
# the GNU General Public License v3.0 or later.
# See the LICENSE file in the project root for details.

cmake_minimum_required(VERSION 3.16)

# ---- Project -----------------------------------------------------------------
project(LLMEngineTest
  DESCRIPTION "Test programs for LLMEngine"
  LANGUAGES CXX)

# C++ standard (no compiler extensions)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
  # Fallback to sibling build artifact for standalone test builds
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
# Not needed: link to LLMEngine propagates includes

# ---- Test executables ---------------------------------------------------------
add_executable(test_llmengine test.cpp)
target_link_libraries(test_llmengine
    PRIVATE
    LLMEngine
    cpr::cpr
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto)

add_executable(test_api test_api.cpp)
target_link_libraries(test_api
    PRIVATE
    LLMEngine
    cpr::cpr
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto)

# ---- Target properties --------------------------------------------------------
set_target_properties(test_llmengine PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/build")

set_target_properties(test_api PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/build")

# ---- Summary -----------------------------------------------------------------
message(STATUS "Test configuration complete")
