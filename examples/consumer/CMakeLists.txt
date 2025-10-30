cmake_minimum_required(VERSION 3.16)
project(LLMEngineConsumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Prefer in-tree targets when built as part of the root; otherwise support standalone usage
get_filename_component(_PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(_PROJECT_BUILD_DIR "${_PROJECT_ROOT}/build")

set(_llm_target "")

# First, prefer an installed/configured package
find_package(LLMEngine QUIET)
if (LLMEngine_FOUND)
  set(_llm_target "llmengine::llmengine")
elseif (TARGET LLMEngine)
  # In-tree build: create a consistent namespaced alias and use it
  string(TOLOWER "LLMEngine" _core_ns)
  if (NOT TARGET ${_core_ns}::${_core_ns})
    add_library(${_core_ns}::${_core_ns} ALIAS LLMEngine)
  endif()
  set(_llm_target "${_core_ns}::${_core_ns}")
else()
  # Standalone example flow: try build-tree package, else fallback to sibling static lib
  set(_HAVE_BUILD_CONFIG OFF)
  if (EXISTS "${_PROJECT_BUILD_DIR}/LLMEngineConfig.cmake" AND EXISTS "${_PROJECT_BUILD_DIR}/LLMEngineTargets.cmake")
    list(PREPEND CMAKE_PREFIX_PATH "${_PROJECT_BUILD_DIR}")
    set(_HAVE_BUILD_CONFIG ON)
  endif()

  if (_HAVE_BUILD_CONFIG)
    find_package(LLMEngine QUIET)
  endif()

  if (NOT LLMEngine_FOUND)
    set(_PROJECT_STATIC_LIB_PATH "${_PROJECT_BUILD_DIR}/libLLMEngine.a")
    if (EXISTS "${_PROJECT_STATIC_LIB_PATH}")
      add_library(LLMEngine STATIC IMPORTED)
      set_target_properties(LLMEngine PROPERTIES
        IMPORTED_LOCATION "${_PROJECT_STATIC_LIB_PATH}")
      target_include_directories(LLMEngine INTERFACE
        "${_PROJECT_ROOT}/src"
        "${_PROJECT_BUILD_DIR}/generated_include")
      string(TOLOWER "LLMEngine" _core_ns)
      add_library(${_core_ns}::${_core_ns} ALIAS LLMEngine)
      set(_llm_target "${_core_ns}::${_core_ns}")
    else()
      message(FATAL_ERROR "Could not find LLMEngine package or sibling build outputs. Build the root project or install LLMEngine.")
    endif()
  else()
    set(_llm_target "llmengine::llmengine")
  endif()
endif()
find_package(nlohmann_json REQUIRED)
if (NOT TARGET cpr::cpr)
  find_package(cpr REQUIRED)
endif()

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE ${_llm_target} nlohmann_json::nlohmann_json cpr::cpr)


