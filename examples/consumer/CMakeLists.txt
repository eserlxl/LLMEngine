cmake_minimum_required(VERSION 3.20)
project(LLMEngineConsumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Prefer in-tree targets when built as part of the root; otherwise support standalone usage
get_filename_component(_PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(_PROJECT_BUILD_DIR "${_PROJECT_ROOT}/build")

set(_llm_target "")

# First, prefer an installed/configured package
find_package(LLMEngine QUIET)
if (LLMEngine_FOUND)
  # When found via package, use the exported target name
  # Check for both possible exported names
  if (TARGET llmengine::LLMEngine)
    set(_llm_target "llmengine::LLMEngine")
  elseif (TARGET llmengine::llmengine)
    set(_llm_target "llmengine::llmengine")
  else()
    message(FATAL_ERROR "LLMEngine package found but no valid target available")
  endif()
elseif (TARGET LLMEngine)
  # In-tree build: use the namespaced alias that matches the main project
  # The main CMakeLists.txt creates llmengine::LLMEngine (capital LLMEngine)
  string(TOLOWER "LLMEngine" _core_ns)
  set(_expected_alias "${_core_ns}::LLMEngine")
  if (NOT TARGET ${_expected_alias})
    add_library(${_expected_alias} ALIAS LLMEngine)
  endif()
  set(_llm_target "${_expected_alias}")
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
      set(_expected_alias "${_core_ns}::LLMEngine")
      add_library(${_expected_alias} ALIAS LLMEngine)
      set(_llm_target "${_expected_alias}")
    else()
      message(FATAL_ERROR "Could not find LLMEngine package or sibling build outputs. Build the root project or install LLMEngine.")
    endif()
  else()
    # Try the standard exported name
    if (TARGET llmengine::LLMEngine)
      set(_llm_target "llmengine::LLMEngine")
    else()
      message(FATAL_ERROR "LLMEngine package found but no valid target available")
    endif()
  endif()
endif()
find_package(nlohmann_json REQUIRED)
if (NOT TARGET cpr::cpr)
  find_package(cpr REQUIRED)
endif()

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE ${_llm_target} llmengine::compile_options nlohmann_json::nlohmann_json cpr::cpr)


