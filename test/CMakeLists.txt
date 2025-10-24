cmake_minimum_required(VERSION 3.14)
project(LLMEngineTest)

set(CMAKE_CXX_STANDARD 17)

# Find dependencies
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(cpr REQUIRED)

# Include the parent directory's src folder for headers
include_directories(../src)

# Create the test executables
add_executable(test_llmengine test.cpp)
add_executable(test_api test_api.cpp)

# Link with the LLMEngine library
target_link_libraries(test_llmengine 
    /opt/lxl/c++/LLMEngine/build/libLLMEngine.a
    cpr::cpr 
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_link_libraries(test_api 
    /opt/lxl/c++/LLMEngine/build/libLLMEngine.a
    cpr::cpr 
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Set the working directory to the project root for relative paths
set_target_properties(test_llmengine PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "../build"
)

set_target_properties(test_api PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "../build"
)
