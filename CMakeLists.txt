cmake_minimum_required(VERSION 3.20)
project(AstraCompiler LANGUAGES CXX)

# Установка стандарта
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Опции для отладки
option(ENABLE_DEBUG "Enable debug build" ON)

# Исходники компилятора
add_subdirectory(src)

# Цель для запуска примеров
add_custom_target(run
    COMMAND ${CMAKE_BINARY_DIR}/astra examples/basics/main.astra
    DEPENDS astra
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Цель для отладки
add_custom_target(debug
    COMMAND gdb ${CMAKE_BINARY_DIR}/astra
    DEPENDS astra
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Цель для очистки
add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/*
)
