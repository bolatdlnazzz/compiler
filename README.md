# Astra Compiler

Компилятор языка программирования **Astra** — статически типизированного компилируемого языка общего назначения.

## Требования

- `g++` или `clang++` с поддержкой C++23
- `cmake` ≥ 3.20
- `nasm` (для компиляции в исполняемый файл)
- `cc` / `clang` (для линковки с runtime)

## Сборка

```bash
mkdir build && cd build
cmake ..
cmake --build . -j
```

Или без cmake (из директории `src/`):

```bash
cd src
g++ -std=c++23 -o astra \
    lexer.cpp parser.cpp semantic.cpp ast_dump.cpp codegen.cpp main.cpp \
    -DASTRA_RUNTIME_C_PATH='"./runtime.c"'
```

## Использование

```bash
# Компиляция в исполняемый файл
./build/src/astra <source.astra> [-o <output>]

# Отладочные режимы
./build/src/astra <source.astra> --dump-tokens   # вывести токены и выйти
./build/src/astra <source.astra> --dump-ast      # вывести AST после парсинга
./build/src/astra <source.astra> --emit-asm      # сгенерировать только .asm

# Пример
./build/src/astra examples/tests/basic.astra -o build/basic
./build/basic
```

## Запуск тестов

```bash
./build/src/astra examples/tests/basic.astra   -o build/basic   && ./build/basic
./build/src/astra examples/tests/arrays.astra  -o build/arrays  && ./build/arrays
./build/src/astra examples/tests/structs.astra -o build/structs && ./build/structs
./build/src/astra examples/tests/floats.astra  -o build/floats  && ./build/floats
./build/src/astra examples/tests/casts.astra   -o build/casts   && ./build/casts
./build/src/astra examples/tests/oob.astra     -o build/oob     && ./build/oob
```

## Структура проекта

```
astra/
├── src/
│   ├── lexer.h / lexer.cpp         — лексический анализатор
│   ├── parser.h / parser.cpp       — синтаксический анализатор (рекурсивный спуск + Pratt)
│   ├── ast.h                       — узлы AST
│   ├── ast_dump.h / ast_dump.cpp   — вывод AST для отладки
│   ├── semantic.h / semantic.cpp   — семантический анализатор, вывод типов
│   ├── codegen.h / codegen.cpp     — кодогенератор x86-64 NASM
│   ├── main.cpp                    — точка входа компилятора
│   └── runtime.c                   — runtime-библиотека (print, input, string, bounds check)
├── specs/
│   ├── grammar.md                  — лексическая и синтаксическая грамматика
│   ├── semantics.md                — семантика конструкций языка
│   ├── types.md                    — система типов
│   └── codegen.md                  — описание кодогенератора
├── examples/
│   └── tests/
│       ├── basic.astra             — функции, if/else, while
│       ├── arrays.astra            — массивы: литерал, индекс, присваивание
│       ├── structs.astra           — структуры: литерал, поле, присваивание
│       ├── floats.astra            — float32/float64 через XMM
│       ├── casts.astra             — явное приведение типов (as)
│       └── oob.astra               — runtime bounds check
├── CMakeLists.txt
├── README.md
└── report.md
```

## Пример программы

```astra
struct Point {
    x: int32;
    y: int32;
}

fn distance_sq(p: Point) -> int32 {
    return p.x * p.x + p.y * p.y;
}

fn main() -> int32 {
    let p = Point { x: 3, y: 4 };
    print(distance_sq(p));   // выведет 25

    let xs = [10, 20, 30];
    print(xs[2]);             // выведет 30

    let a: float64 = 1.5;
    let b: float64 = 2.5;
    print(a + b);             // выведет 4

    return 0;
}
```

## Ключевые особенности языка

- Статическая типизация, вывод типов для `let`-переменных
- `let` — иммутабельные переменные, `var` — мутабельные
- Массивы фиксированного размера, размер является частью типа: `[int32; 10]`
- Пользовательские структуры, пространства имён, синонимы типов
- Явное приведение типов через `as`: `x as float64`
- Runtime-проверки: деление на ноль, выход за границы массива
- Условия `if`/`else` и цикл `while` со скобками вокруг условия
