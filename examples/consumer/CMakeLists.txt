cmake_minimum_required(VERSION 3.16)
project(LLMEngineConsumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(LLMEngine REQUIRED)
find_package(nlohmann_json REQUIRED)
if (NOT TARGET cpr::cpr)
  find_package(cpr REQUIRED)
endif()

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE llmengine::llmengine nlohmann_json::nlohmann_json cpr::cpr)


