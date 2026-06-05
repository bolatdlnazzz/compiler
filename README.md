# Astra Compiler

**Astra** — учебный компилятор собственного статически типизированного языка программирования.  
Компилятор реализован на C++23 и генерирует x86-64 NASM-код с последующей сборкой в исполняемый файл.

## Что реализовано

Компилятор проходит полный pipeline:

```text
исходный файл .astra
    ↓
Lexer        → поток токенов
    ↓
Parser       → AST
    ↓
Semantic     → проверка имён, типов и main
    ↓
Codegen      → x86-64 NASM
    ↓
nasm + cc    → исполняемый файл
```

Основные возможности языка:

- функции с параметрами и возвращаемым значением;
- обязательная точка входа `fn main() -> int32/int64`;
- переменные `let` и `var`;
- статическая типизация;
- вывод типов для `let`-переменных;
- целые типы `int8..int64`, `uint8..uint64`;
- вещественные типы `float32`, `float64`;
- типы `bool`, `string`, `char`;
- массивы фиксированного размера;
- структуры;
- пространства имён и синонимы типов;
- арифметические, логические операции и сравнения;
- явное приведение типов через `as`;
- `if/else`, `while`, `break`, `continue`, `return`;
- встроенные функции `print`, `input`, `len`, `exit`, `panic`, `assert`;
- runtime-проверки деления на ноль и выхода за границы массива.

Дополнительно реализовано:

- Pratt parser для выражений;
- режим `--dump-tokens`;
- режим `--dump-ast`;
- режим `--emit-asm`;
- частичное восстановление после синтаксических ошибок;
- поддержка литералов `0x...`, `0b...`, `1e3`, `1.5e-2`, `inf`, `NaN`;
- поддержка символьных литералов: `'a'`, `'\n'`, `'\t'`, `'\\'`, `'\''`, `'\0'`.

## Требования

Для сборки нужны:

- `g++` или `clang++` с поддержкой C++23;
- `cmake` версии 3.20 или выше;
- `nasm` для сборки ассемблерного файла;
- `cc` или `clang` для линковки с runtime-библиотекой.

## Сборка

Из корня проекта:

```bash
cmake -B build
make -C build
```

Альтернативный вариант:

```bash
cmake -S . -B build
cmake --build build -j
```

После сборки исполняемый файл компилятора находится здесь:

```bash
./build/src/astra
```

## Быстрый запуск основного примера

Скомпилировать программу на Astra:

```bash
./build/src/astra examples/tests/basic.astra -o build/basic
```

Запустить полученный исполняемый файл:

```bash
./build/basic
```

Ожидаемый вывод для текущего `examples/tests/basic.astra`:

```text
25
20
100
0
1
2
```

## Отладочные режимы

Показать токены после работы лексера:

```bash
./build/src/astra examples/tests/basic.astra --dump-tokens
```

Показать AST после парсинга:

```bash
./build/src/astra examples/tests/basic.astra --dump-ast
```

Сгенерировать только `.asm` без линковки:

```bash
./build/src/astra examples/tests/basic.astra --emit-asm -o build/basic.asm
```

Посмотреть сгенерированный ассемблер:

```bash
cat build/basic.asm
```

## Пример программы на Astra

```astra
fn square(x: int32) -> int32 {
    return x * x;
}

fn main() -> int32 {
    let a: int32 = 5;
    print(square(a));

    let xs = [10, 20, 30];
    print(xs[1]);

    if (a > 3) {
        print(100);
    } else {
        print(0);
    }

    var i: int32 = 0;
    while (i < 3) {
        print(i);
        i = i + 1;
    }

    return 0;
}
```

## Демонстрация требований v1.0

Пример с новыми базовыми возможностями:

```astra
fn main() -> int32 {
    let c: char = 'A';
    print(c);

    let h: int32 = 0x2A;
    let b: int32 = 0b101010;

    print(h);
    print(b);

    let xs = [10, 20, 30];
    print(len(xs));

    let x: float64 = 1.5e2;
    print(x);

    assert(h == b);

    return 0;
}
```

## Проверка ошибок

Для проверки синтаксической ошибки можно использовать файл:

```bash
./build/src/astra test_parser_error.astra
```

Для проверки runtime-ошибки можно создать пример с выходом за границы массива:

```astra
fn main() -> int32 {
    let xs = [1, 2, 3];
    print(xs[10]);
    return 0;
}
```

Компилятор сгенерирует программу, а во время выполнения runtime-библиотека завершит её с сообщением об ошибке.

## Структура проекта

```text
compiler/
├── src/
│   ├── lexer.h / lexer.cpp          — лексический анализатор
│   ├── parser.h / parser.cpp        — синтаксический анализатор
│   ├── ast.h                        — узлы AST
│   ├── ast_dump.h / ast_dump.cpp    — вывод AST
│   ├── semantic.h / semantic.cpp    — семантический анализатор
│   ├── codegen.h / codegen.cpp      — генератор x86-64 NASM
│   ├── main.cpp                     — CLI и запуск фаз компилятора
│   ├── runtime.c                    — runtime-библиотека
│   └── CMakeLists.txt               — сборка исполняемого файла astra
├── specs/
│   ├── grammar.md                   — грамматика языка
│   ├── semantics.md                 — семантика конструкций
│   ├── types.md                     — система типов
│   └── codegen.md                   — описание кодогенерации
├── examples/
│   └── tests/
│       └── basic.astra              — основной пример для демонстрации
├── build/                           — директория сборки
├── CMakeLists.txt                   — корневой CMake-файл
├── README.md                        — описание проекта
├── report.md                        — отчёт
├── projectv0.1.md                   — исходное ТЗ
└── test_parser_error.astra          — пример ошибки парсера
```

## Команды для защиты

Минимальный набор команд, который показывает работу проекта:

```bash
cmake -B build
make -C build
```

```bash
./build/src/astra examples/tests/basic.astra --dump-tokens
```

```bash
./build/src/astra examples/tests/basic.astra --dump-ast
```

```bash
./build/src/astra examples/tests/basic.astra --emit-asm -o build/basic.asm
```

```bash
./build/src/astra examples/tests/basic.astra -o build/basic
./build/basic
```

## Известные ограничения

- IR-фаза и оптимизатор не реализованы.
- Полноценная модульная система с импортом/экспортом файлов не реализована.
- `print` предназначен в основном для скалярных типов и строк.
- Кодогенератор ориентирован на x86-64 NASM и System V AMD64 ABI.
