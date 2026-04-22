set(SOURCES
    lexer/Lexer.cpp
    parser/Parser.cpp
    semantic/Semantic.cpp
    ir/IR.cpp
    codegen/Codegen.cpp
    runtime/Runtime.cpp
    main.cpp
)

add_executable(astra ${SOURCES})

target_include_directories(astra PRIVATE ${CMAKE_SOURCE_DIR}/src)
