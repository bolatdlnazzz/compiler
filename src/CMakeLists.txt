set(SOURCES
    lexer.cpp
    parser.cpp
    semantic.cpp
    main.cpp
)

add_executable(astra ${SOURCES})

target_include_directories(astra PRIVATE ${CMAKE_SOURCE_DIR}/src)
