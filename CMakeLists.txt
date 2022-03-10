cmake_minimum_required(VERSION 3.10)

# set the project name
project(JSON-stuff)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_COMPILER_VERSION 10)

add_subdirectory(libs)

include_directories(headers)

add_executable(json-stitcher ./src/main.cpp)
target_link_libraries(json-stitcher PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(json-stitcher PRIVATE nlohmann_json_schema_validator)
target_link_libraries(json-stitcher PRIVATE cpr::cpr)
target_link_libraries(json-stitcher PRIVATE base64)
target_link_libraries(json-stitcher PRIVATE cxxopts)
