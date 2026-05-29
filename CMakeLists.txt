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

# Удобная цель-алиас для ТЗ. Обычная сборка также работает через: cmake --build .
add_custom_target(build DEPENDS astra)

add_custom_target(run
    COMMAND $<TARGET_FILE:astra> ${CMAKE_SOURCE_DIR}/examples/tests/basic.astra --emit-asm -o ${CMAKE_BINARY_DIR}/basic.asm
    DEPENDS astra
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_custom_target(debug
    COMMAND gdb $<TARGET_FILE:astra>
    DEPENDS astra
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Цель clean создаётся CMake автоматически: cmake --build . --target clean
add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/*
)
