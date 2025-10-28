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

# ---- Include directories ------------------------------------------------------
include_directories(../src)

# ---- Test executables ---------------------------------------------------------
add_executable(test_llmengine test.cpp)
target_link_libraries(test_llmengine 
    /opt/lxl/c++/LLMEngine/build/libLLMEngine.a
    cpr::cpr 
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto)

add_executable(test_api test_api.cpp)
target_link_libraries(test_api 
    /opt/lxl/c++/LLMEngine/build/libLLMEngine.a
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
