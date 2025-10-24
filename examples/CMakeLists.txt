cmake_minimum_required(VERSION 3.14)
project(LLMEngineExamples)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(NLOHMANN_JSON REQUIRED nlohmann_json)

# Find CPR manually since it doesn't provide pkg-config
find_library(CPR_LIBRARY cpr PATHS /usr/lib)
find_path(CPR_INCLUDE_DIR cpr/cpr.h PATHS /usr/include)

if(NOT CPR_LIBRARY OR NOT CPR_INCLUDE_DIR)
    message(FATAL_ERROR "CPR library not found. Please install cpr-git from AUR.")
endif()

# Include directories
include_directories(../src)
include_directories(${CPR_INCLUDE_DIR})
include_directories(${NLOHMANN_JSON_INCLUDE_DIRS})

# Link directories
link_directories(../build)
link_directories(${NLOHMANN_JSON_LIBRARY_DIRS})

# Add executable for chatbot
add_executable(chatbot chatbot.cpp)
target_link_libraries(chatbot LLMEngine ${CPR_LIBRARY} ${NLOHMANN_JSON_LIBRARIES} ssl crypto pthread curl)

# Add executable for code analyzer
add_executable(code_analyzer code_analyzer.cpp)
target_link_libraries(code_analyzer LLMEngine ${CPR_LIBRARY} ${NLOHMANN_JSON_LIBRARIES} ssl crypto pthread curl)

# Add executable for text processor
add_executable(text_processor text_processor.cpp)
target_link_libraries(text_processor LLMEngine ${CPR_LIBRARY} ${NLOHMANN_JSON_LIBRARIES} ssl crypto pthread curl)

# Add executable for file analyzer
add_executable(file_analyzer file_analyzer.cpp)
target_link_libraries(file_analyzer LLMEngine ${CPR_LIBRARY} ${NLOHMANN_JSON_LIBRARIES} ssl crypto pthread curl)

# Add executable for translation tool
add_executable(translator translator.cpp)
target_link_libraries(translator LLMEngine ${CPR_LIBRARY} ${NLOHMANN_JSON_LIBRARIES} ssl crypto pthread curl)
