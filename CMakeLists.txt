cmake_minimum_required(VERSION 3.23)

project(Stocc
    VERSION 1.0.0
    LANGUAGES CXX
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/stocc/project.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/stocc/project.hpp
    @ONLY
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

include(FetchContent)

set(BOOST_INCLUDE_LIBRARIES
    accumulators
    algorithm
    program_options
)

FetchContent_Declare(
    Boost
    GIT_REPOSITORY https://github.com/boostorg/boost.git
    GIT_TAG boost-1.90.0
)

FetchContent_MakeAvailable(Boost)

set(SPDLOG_USE_STD_FORMAT TRUE)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.17.0
)

FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
)

FetchContent_MakeAvailable(tomlplusplus)

file(GLOB_RECURSE PROJECT_SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCE_FILES})

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME stocc
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

add_library(dependencies INTERFACE)
target_include_directories(dependencies INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(dependencies INTERFACE
    Boost::accumulators
    Boost::algorithm
    Boost::program_options
    spdlog::spdlog
    tomlplusplus::tomlplusplus
)

target_link_libraries(${PROJECT_NAME} PRIVATE dependencies)

enable_testing()

function (create_test name)
    add_executable(${name}_test tests/${name}_test.cpp)
    target_link_libraries(${name}_test PRIVATE dependencies)
    target_include_directories(${name}_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    add_test(NAME ${name}_test COMMAND ${name}_test)
endfunction()

create_test(deviation)
create_test(mean)
create_test(median)
create_test(variance)
