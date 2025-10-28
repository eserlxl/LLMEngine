# Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of LLMEngine and is licensed under
# the GNU General Public License v3.0 or later.
# See the LICENSE file in the project root for details.

cmake_minimum_required(VERSION 3.16)

# ---- Project -----------------------------------------------------------------
project(LLMEngineExamples
  DESCRIPTION "Example programs demonstrating LLMEngine capabilities"
  LANGUAGES CXX)

# C++ standard (no compiler extensions)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Dependencies ------------------------------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(NLOHMANN_JSON REQUIRED nlohmann_json)

# Find CPR manually since it doesn't provide pkg-config
find_library(CPR_LIBRARY cpr PATHS /usr/lib)
find_path(CPR_INCLUDE_DIR cpr/cpr.h PATHS /usr/include)

if(NOT CPR_LIBRARY OR NOT CPR_INCLUDE_DIR)
    message(FATAL_ERROR "CPR library not found. Please install cpr-git from AUR.")
endif()

# ---- Include directories ------------------------------------------------------
include_directories(../src)
include_directories(${CPR_INCLUDE_DIR})
include_directories(${NLOHMANN_JSON_INCLUDE_DIRS})

# ---- Link directories --------------------------------------------------------
link_directories(../build)
link_directories(${NLOHMANN_JSON_LIBRARY_DIRS})

# ---- Executables -------------------------------------------------------------
add_executable(chatbot chatbot.cpp)
target_link_libraries(chatbot 
  LLMEngine 
  ${CPR_LIBRARY} 
  ${NLOHMANN_JSON_LIBRARIES} 
  ssl crypto pthread curl)

add_executable(code_analyzer code_analyzer.cpp)
target_link_libraries(code_analyzer 
  LLMEngine 
  ${CPR_LIBRARY} 
  ${NLOHMANN_JSON_LIBRARIES} 
  ssl crypto pthread curl)

add_executable(text_processor text_processor.cpp)
target_link_libraries(text_processor 
  LLMEngine 
  ${CPR_LIBRARY} 
  ${NLOHMANN_JSON_LIBRARIES} 
  ssl crypto pthread curl)

add_executable(file_analyzer file_analyzer.cpp)
target_link_libraries(file_analyzer 
  LLMEngine 
  ${CPR_LIBRARY} 
  ${NLOHMANN_JSON_LIBRARIES} 
  ssl crypto pthread curl)

add_executable(translator translator.cpp)
target_link_libraries(translator 
  LLMEngine 
  ${CPR_LIBRARY} 
  ${NLOHMANN_JSON_LIBRARIES} 
  ssl crypto pthread curl)

add_executable(logo_generator logo_generator.cpp)
target_link_libraries(logo_generator 
  LLMEngine 
  ${CPR_LIBRARY} 
  ${NLOHMANN_JSON_LIBRARIES} 
  ssl crypto pthread curl)

add_executable(image_generator image_generator.cpp)
target_link_libraries(image_generator 
  LLMEngine 
  ${CPR_LIBRARY} 
  ${NLOHMANN_JSON_LIBRARIES} 
  ssl crypto pthread curl)

add_executable(readme_chatbot readme_chatbot.cpp)
target_link_libraries(readme_chatbot 
  LLMEngine 
  ${CPR_LIBRARY} 
  ${NLOHMANN_JSON_LIBRARIES} 
  ssl crypto pthread curl)

add_executable(multi_bot_analyzer multiBotAnalyzer.cpp)
target_link_libraries(multi_bot_analyzer 
  LLMEngine 
  ${CPR_LIBRARY} 
  ${NLOHMANN_JSON_LIBRARIES} 
  ssl crypto pthread curl)

# ---- Summary -----------------------------------------------------------------
message(STATUS "Examples configuration complete")
