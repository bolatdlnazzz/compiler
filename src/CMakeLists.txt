set(SOURCES
    lexer.cpp
    parser.cpp
    semantic.cpp
    ast_dump.cpp
    codegen.cpp
    main.cpp
)

add_executable(astra ${SOURCES})

target_compile_features(astra PRIVATE cxx_std_23)
target_include_directories(astra PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(astra PRIVATE ASTRA_RUNTIME_C_PATH="${CMAKE_CURRENT_SOURCE_DIR}/runtime.c")
