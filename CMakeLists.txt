cmake_minimum_required(VERSION 3.23)

project(Stocc
    VERSION 1.0.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE PROJECT_SOURCE_FILES "src/*.cpp" "src/*.hpp")

add_executable(${PROJECT_NAME} ${PROJECT_SOURCE_FILES})

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "stocc"
)
