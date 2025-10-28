# Copyright © 2025 Eser KUBALI <lxldev.contact@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of LLMEngine and is licensed under
# the GNU General Public License v3.0 or later.
# See the LICENSE file in the project root for details.

cmake_minimum_required(VERSION 3.14)
project(LLMEngine)

set(CMAKE_CXX_STANDARD 17)

# Find dependencies
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(cpr REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/src)

# Build the LLMEngine library
add_library(LLMEngine STATIC
    src/LLMEngine.cpp
    src/LLMOutputProcessor.cpp
    src/Utils.cpp
    src/APIClient.cpp
)

target_link_libraries(LLMEngine PUBLIC cpr::cpr nlohmann_json::nlohmann_json)

target_include_directories(LLMEngine PUBLIC ${CMAKE_SOURCE_DIR}/src)

# Optionally, build an executable for testing or CLI (uncomment and provide main.cpp if needed)
# add_executable(llmEngine src/main.cpp)
# target_link_libraries(llmEngine PRIVATE LLMEngine)

# Install library
install(TARGETS LLMEngine DESTINATION lib)

# Install public headers
install(FILES
    src/LLMEngine.hpp
    src/LLMOutputProcessor.hpp
    src/Utils.hpp
    src/APIClient.hpp
    DESTINATION include/LLMEngine
)

# Install config and scripts (if these directories exist)
install(DIRECTORY config/ DESTINATION share/llmEngine/config OPTIONAL)
install(DIRECTORY scripts/ DESTINATION share/llmEngine/scripts OPTIONAL)
install(DIRECTORY prompts/ DESTINATION share/llmEngine/prompts OPTIONAL)
install(DIRECTORY reports/ DESTINATION share/llmEngine/reports OPTIONAL)

# Define default config path as a compile-time constant
set(CONFIG_INSTALL_PATH "/usr/share/llmEngine/config/settings.json")
add_compile_definitions(DEFAULT_CONFIG_PATH="${CONFIG_INSTALL_PATH}")

# Define default reports and prompts path as compile-time constants
set(REPORTS_INSTALL_PATH "/usr/share/llmEngine/reports")
add_compile_definitions(DEFAULT_REPORTS_PATH="${REPORTS_INSTALL_PATH}")
set(PROMPTS_INSTALL_PATH "/usr/share/llmEngine/prompts")
add_compile_definitions(DEFAULT_PROMPTS_PATH="${PROMPTS_INSTALL_PATH}")