cmake_minimum_required(VERSION 3.16)
project(LLMEngineConsumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Try to locate the package configuration using generic variables first.
# If not available, fall back to sibling build outputs to keep this example self-contained.
get_filename_component(_PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(_PROJECT_BUILD_DIR "${_PROJECT_ROOT}/build")

if (EXISTS "${_PROJECT_BUILD_DIR}/LLMEngineConfig.cmake")
  list(PREPEND CMAKE_PREFIX_PATH "${_PROJECT_BUILD_DIR}")
endif()

find_package(LLMEngine QUIET)
if (NOT LLMEngine_FOUND)
  set(_PROJECT_STATIC_LIB_PATH "${_PROJECT_BUILD_DIR}/libLLMEngine.a")
  if (EXISTS "${_PROJECT_STATIC_LIB_PATH}")
    add_library(LLMEngine STATIC IMPORTED)
    set_target_properties(LLMEngine PROPERTIES
      IMPORTED_LOCATION "${_PROJECT_STATIC_LIB_PATH}")
    target_include_directories(LLMEngine INTERFACE
      "${_PROJECT_ROOT}/src"
      "${_PROJECT_BUILD_DIR}/generated_include")
    add_library(llmengine::llmengine ALIAS LLMEngine)
  else()
    message(FATAL_ERROR "Could not find LLMEngine package or sibling build outputs. Build the root project or install LLMEngine.")
  endif()
endif()
find_package(nlohmann_json REQUIRED)
if (NOT TARGET cpr::cpr)
  find_package(cpr REQUIRED)
endif()

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE llmengine::llmengine nlohmann_json::nlohmann_json cpr::cpr)


