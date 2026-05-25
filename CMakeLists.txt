cmake_minimum_required(VERSION 3.20)
project(AstraCompiler LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(ENABLE_DEBUG "Enable debug build" ON)

add_subdirectory(src)

add_custom_target(run
    COMMAND ${CMAKE_BINARY_DIR}/src/astra ${CMAKE_SOURCE_DIR}/examples/basics/main.astra
    DEPENDS astra
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_custom_target(debug
    COMMAND gdb ${CMAKE_BINARY_DIR}/src/astra
    DEPENDS astra
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/*
)
