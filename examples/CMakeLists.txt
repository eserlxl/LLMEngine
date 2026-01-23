# Copyright © 2026 Eser KUBALI <lxldev.contact@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This CMake file configures building the LLMEngine example programs, and is
# licensed under the GNU General Public License v3.0 or later.
# See the LICENSE file in the project root for details.

cmake_minimum_required(VERSION 3.31)

# ---- Project -----------------------------------------------------------------
project(LLMEngineExamples
  DESCRIPTION "Example programs demonstrating LLMEngine capabilities"
  LANGUAGES CXX)

# Build system quality-of-life
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Do not set global C++ standard; set per-target features instead

# ---- Options (inherit from root if present; otherwise define defaults) ---------
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

# Use default generator directories; avoid overriding global runtime output dir

# ---- Dependencies ------------------------------------------------------------
# If building within the same source tree, prefer existing core target directly
if (TARGET llmengine::LLMEngine)
  if (NOT TARGET LLMEngine)
    add_library(LLMEngine ALIAS llmengine::LLMEngine)
  endif()
elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../build/${CMAKE_PROJECT_NAME}Targets.cmake" OR
        EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../build/LLMEngineTargets.cmake")
  list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../build")
  # Try to consume LLMEngine package first (from build tree or system install)
  find_package(LLMEngine CONFIG QUIET)
endif()

if (TARGET LLMEngine)
  # When consuming the exported target, its PUBLIC deps (nlohmann_json, etc.)
  # and compile options propagate, so no extra package lookups are required here.
else()
  # Standalone fallback: require headers/libs that examples need directly
  find_package(OpenSSL REQUIRED)
  find_package(nlohmann_json REQUIRED)
  if (NOT TARGET cpr::cpr AND NOT TARGET cpr)
    find_package(cpr REQUIRED)
  endif()
  if (NOT TARGET cpr::cpr AND TARGET cpr)
    add_library(cpr::cpr ALIAS cpr)
  endif()

  # Provide a local imported LLMEngine target from sibling build output
  set(_LLMENGINE_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../build/libLLMEngine.a")
  if (EXISTS "${_LLMENGINE_LIB_PATH}")
    add_library(LLMEngine STATIC IMPORTED)
    set_target_properties(LLMEngine PROPERTIES IMPORTED_LOCATION "${_LLMENGINE_LIB_PATH}")
    target_include_directories(LLMEngine INTERFACE
      "${CMAKE_CURRENT_SOURCE_DIR}/../include"
      "${CMAKE_CURRENT_SOURCE_DIR}/../build/generated_include"
      "${CMAKE_CURRENT_SOURCE_DIR}/../build/generated_include/LLMEngine")
    # Provide interface target for compile options and propagate deps
    add_library(llmengine_compile_options INTERFACE)
    add_library(llmengine::compile_options ALIAS llmengine_compile_options)
    # Populate fallback interface with common hardening/warning flags
    target_compile_features(llmengine_compile_options INTERFACE cxx_std_23)
    target_compile_options(llmengine_compile_options INTERFACE -fstack-protector-strong)
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
      target_link_options(llmengine_compile_options INTERFACE -Wl,-z,relro,-z,now)
    endif()
    target_compile_definitions(llmengine_compile_options INTERFACE _FORTIFY_SOURCE=2)
    if (WARNING_MODE)
      target_compile_options(llmengine_compile_options INTERFACE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wnon-virtual-dtor
        -Wold-style-cast -Woverloaded-virtual -Wnull-dereference -Wformat=2 -Wimplicit-fallthrough)
      if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(llmengine_compile_options INTERFACE -Wduplicated-cond -Wlogical-op -Wuseless-cast)
      endif()
      if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(llmengine_compile_options INTERFACE -Wextra-semi -Wno-c99-extensions)
      endif()
    endif()
    set_target_properties(LLMEngine PROPERTIES
      INTERFACE_LINK_LIBRARIES "nlohmann_json::nlohmann_json")
    # If nlohmann_json was fetched by the root build, expose its include dir to examples
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../build/_deps/nlohmann_json-src/single_include")
      target_include_directories(LLMEngine INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/../build/_deps/nlohmann_json-src/single_include")
    endif()
  else()
    message(FATAL_ERROR "LLMEngine target not found. Build the root project first or install LLMEngine.")
  endif()
endif()

# ---- Include directories ------------------------------------------------------
# Not needed: targets propagate include paths

# ---- Common build flags (mirror root CMake) -----------------------------------
function(apply_common_build_flags target_name)
  if (MSVC)
    if (DEBUG_MODE)
      target_compile_options(${target_name} PRIVATE /Od /Z7)
      target_compile_definitions(${target_name} PRIVATE DEBUG)
    elseif (PERFORMANCE_BUILD)
      target_compile_options(${target_name} PRIVATE /O2)
      target_compile_definitions(${target_name} PRIVATE NDEBUG)
    else()
      target_compile_options(${target_name} PRIVATE /O2 /Z7)
    endif()
  else()
    target_compile_options(${target_name} PRIVATE -fstack-protector-strong)
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
      target_link_options(${target_name} PRIVATE -Wl,-z,relro,-z,now)
    endif()
    if (DEBUG_MODE)
      target_compile_options(${target_name} PRIVATE -O0 -g -fno-omit-frame-pointer)
      target_compile_definitions(${target_name} PRIVATE DEBUG)
      if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_definitions(${target_name} PRIVATE _GLIBCXX_ASSERTIONS)
      endif()
      if (ENABLE_SANITIZERS)
        if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
          target_compile_options(${target_name} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
          target_link_options(${target_name} PRIVATE -fsanitize=address,undefined)
        endif()
      endif()
    elseif (PERFORMANCE_BUILD)
      target_compile_options(${target_name} PRIVATE -O3)
      if (ENABLE_NATIVE_OPTIMIZATION)
        target_compile_options(${target_name} PRIVATE -march=native -mtune=native)
      endif()
      target_compile_definitions(${target_name} PRIVATE NDEBUG _FORTIFY_SOURCE=2)
    else()
      target_compile_options(${target_name} PRIVATE -O2 -g -fno-omit-frame-pointer)
      target_compile_definitions(${target_name} PRIVATE _FORTIFY_SOURCE=2)
    endif()
    if (WARNING_MODE)
      target_compile_options(${target_name} PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wnon-virtual-dtor
        -Wold-style-cast -Woverloaded-virtual -Wnull-dereference -Wformat=2 -Wimplicit-fallthrough)
      if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${target_name} PRIVATE -Wduplicated-cond -Wlogical-op -Wuseless-cast)
      endif()
      if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${target_name} PRIVATE -Wextra-semi -Wno-c99-extensions)
      endif()
    endif()
  endif()
endfunction()

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

# ---- Link directories --------------------------------------------------------
# Not needed with imported targets

# ---- Dependencies ------------------------------------------------------------
# Do not require OpenSSL directly; rely on transitive requirements via cpr

# ---- Executables -------------------------------------------------------------
add_executable(chatbot chatbot.cpp)
target_link_libraries(chatbot
  PRIVATE
    LLMEngine
    llmengine::compile_options
    cpr::cpr
    nlohmann_json::nlohmann_json)
target_compile_features(chatbot PRIVATE cxx_std_23)
apply_common_build_flags(chatbot)

add_executable(code_analyzer code_analyzer.cpp)
target_link_libraries(code_analyzer
  PRIVATE
    LLMEngine
    llmengine::compile_options
    cpr::cpr
    nlohmann_json::nlohmann_json)
target_compile_features(code_analyzer PRIVATE cxx_std_23)
apply_common_build_flags(code_analyzer)

add_executable(text_processor text_processor.cpp)
target_link_libraries(text_processor
  PRIVATE
    LLMEngine
    llmengine::compile_options
    cpr::cpr
    nlohmann_json::nlohmann_json)
target_compile_features(text_processor PRIVATE cxx_std_23)
apply_common_build_flags(text_processor)

add_executable(file_analyzer file_analyzer.cpp)
target_link_libraries(file_analyzer
  PRIVATE
    LLMEngine
    llmengine::compile_options
    cpr::cpr
    nlohmann_json::nlohmann_json)
target_compile_features(file_analyzer PRIVATE cxx_std_23)
apply_common_build_flags(file_analyzer)

add_executable(translator translator.cpp)
target_link_libraries(translator
  PRIVATE
    LLMEngine
    llmengine::compile_options
    cpr::cpr
    nlohmann_json::nlohmann_json)
target_compile_features(translator PRIVATE cxx_std_23)
apply_common_build_flags(translator)

add_executable(logo_generator logo_generator.cpp)
target_link_libraries(logo_generator
  PRIVATE
    LLMEngine
    llmengine::compile_options
    cpr::cpr
    nlohmann_json::nlohmann_json)
target_compile_features(logo_generator PRIVATE cxx_std_23)
apply_common_build_flags(logo_generator)

add_executable(image_generator image_generator.cpp)
target_link_libraries(image_generator
  PRIVATE
    LLMEngine
    llmengine::compile_options
    cpr::cpr
    nlohmann_json::nlohmann_json)
target_compile_features(image_generator PRIVATE cxx_std_23)
apply_common_build_flags(image_generator)

add_executable(readme_chatbot readme_chatbot.cpp)
target_link_libraries(readme_chatbot
  PRIVATE
    LLMEngine
    llmengine::compile_options
    cpr::cpr
    nlohmann_json::nlohmann_json)
target_compile_features(readme_chatbot PRIVATE cxx_std_23)
apply_common_build_flags(readme_chatbot)

add_executable(batch_processor batch_processor.cpp)
target_link_libraries(batch_processor
  PRIVATE
    LLMEngine
    llmengine::compile_options
    cpr::cpr
    nlohmann_json::nlohmann_json)
target_compile_features(batch_processor PRIVATE cxx_std_23)
apply_common_build_flags(batch_processor)

# ---- Summary -----------------------------------------------------------------
message(STATUS "========== Examples Build Configuration ==========")
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
message(STATUS "Examples configuration complete")

# Optional consumer example using find_package; not built by default
if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/consumer/CMakeLists.txt")
  add_subdirectory(consumer EXCLUDE_FROM_ALL)
endif()
