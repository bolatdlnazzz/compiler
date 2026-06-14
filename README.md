# Astra Compiler

Astra — это учебный компилятор моего собственного статически типизированного языка программирования.
Проект написан на C++23. Компилятор берёт файл `.astra`, разбирает его, проверяет семантику и генерирует x86-64 NASM-код, который потом собирается через `nasm` и `cc` в обычный исполняемый файл.

## Общая схема работы

```text
исходный файл .astra
        ↓
Lexer   → поток токенов
        ↓
Parser  → AST
        ↓
Semantic Analyzer → проверка имён, типов, main, return, break/continue
        ↓
Codegen → x86-64 NASM
        ↓
nasm + cc + runtime.c
        ↓
исполняемый файл
```

Фазы компилятора разделены по файлам в `src/`:

```text
src/
├── lexer.h / lexer.cpp          — лексический анализатор
├── parser.h / parser.cpp        — синтаксический анализатор
├── ast.h                        — структуры AST
├── ast_dump.h / ast_dump.cpp    — вывод AST для отладки
├── semantic.h / semantic.cpp    — семантический анализатор
├── codegen.h / codegen.cpp      — генератор NASM-кода
├── runtime.c                    — runtime-функции для print, input, assert, ошибок
├── main.cpp                     — CLI и запуск pipeline
└── CMakeLists.txt
```

## Что поддерживает язык

Основная база:

- функция входа `fn main() -> int32` или `fn main() -> int64`;
- переменные `let` и `var`;
- функции с параметрами и возвращаемым значением;
- типы `int8/int16/int32/int64`, `uint8/uint16/uint32/uint64`;
- типы `float32`, `float64`, `bool`, `char`, `string`, `unit`;
- массивы фиксированного размера;
- структуры;
- синонимы типов через `type`;
- пространства имён `namespace`;
- арифметика, сравнения, логические операции;
- явное приведение типов через `as`;
- `if/else`, `while`, `break`, `continue`, `return`;
- встроенные функции `print`, `input`, `len`, `exit`, `panic`, `assert`;
- runtime-проверка деления на ноль;
- runtime-проверка выхода за границы массива;
- режимы `--dump-tokens`, `--dump-ast`, `--emit-asm`.

Дополнительно реализовано:

- вывод типа для `let`;
- вывод возвращаемого типа функции по `return`, если `-> Type` не указан;
- перегрузка функций;
- default-параметры;
- именованные аргументы;
- методы структур вида `fn Point.sum(self: Point) -> int32`;
- `if` как выражение;
- битовые операции `~`, `&`, `|`, `^`, `<<`, `>>`;
- конвейерный оператор `|>`;
- блочные комментарии `/* ... */`;
- `module` в начале файла;
- метафункции `sizeof`, `typeid`, `typeof`;
- литералы `0x...`, `0b...`, `1e3`, `1.5e-2`, `inf`, `NaN`;
- символьные литералы `'A'`, `'\n'`, `'\t'`, `'\\'`, `'\''`, `'\0'`.

## Требования для запуска

Нужны:

- CMake 3.20 или новее;
- компилятор C++ с поддержкой C++23, например `g++` или `clang++`;
- `nasm`;
- `cc` или `clang` для линковки.

## Сборка проекта

Из корня проекта:

```bash
cmake -S . -B build
cmake --build build -j
```

После сборки компилятор находится здесь:

```bash
./build/src/astra
```

## Быстрая проверка

Скомпилировать и запустить самый простой пример:

```bash
mkdir -p build/test_bin
./build/src/astra tests/valids/hello.astra -o build/test_bin/hello
./build/test_bin/hello
```

Ожидаемый вывод:

```text
Hello, Astra
```

Пример с арифметикой:

```bash
./build/src/astra tests/valids/math.astra -o build/test_bin/math
./build/test_bin/math
```

Ожидаемый вывод:

```text
14
20
```

## Отладочные режимы

Вывести токены после лексера:

```bash
./build/src/astra tests/valids/hello.astra --dump-tokens
```

Вывести AST после парсера:

```bash
./build/src/astra tests/valids/hello.astra --dump-ast
```

Сгенерировать только `.asm`, без линковки:

```bash
./build/src/astra tests/valids/math.astra --emit-asm -o build/math.asm
```

Посмотреть сгенерированный asm:

```bash
cat build/math.asm
```

## Основные команды для защиты

Сборка:

```bash
cmake -S . -B build
cmake --build build -j
```

Показать токены:

```bash
./build/src/astra tests/valids/hello.astra --dump-tokens
```

Показать AST:

```bash
./build/src/astra tests/valids/hello.astra --dump-ast
```

Показать генерацию asm:

```bash
./build/src/astra tests/valids/math.astra --emit-asm -o build/math.asm
cat build/math.asm
```

Собрать и запустить программу:

```bash
./build/src/astra tests/valids/math.astra -o build/math
./build/math
```

## Проверка базовых valid-тестов

Эти тесты должны успешно компилироваться и запускаться:

```bash
mkdir -p build/test_bin

for f in \
tests/valids/hello.astra \
tests/valids/math.astra \
tests/valids/array.astra \
tests/valids/point.astra \
tests/valids/functions.astra \
tests/valids/branch.astra \
tests/valids/loop.astra \
tests/valids/cast.astra \
tests/valids/values.astra \
tests/valids/alias.astra \
tests/valids/check.astra

do
    name=$(basename "$f" .astra)
    echo "BUILD AND RUN: $f"
    ./build/src/astra "$f" -o "build/test_bin/$name" || exit 1
    "./build/test_bin/$name" || exit 1
    echo
done
```

## Проверка дополнительных заданий

```bash
mkdir -p build/test_bin

for f in \
tests/valids/dop_bitwise.astra \
tests/valids/dop_block_comment.astra \
tests/valids/dop_default_named_args.astra \
tests/valids/dop_if_expr.astra \
tests/valids/dop_meta.astra \
tests/valids/dop_module.astra \
tests/valids/dop_overload.astra \
tests/valids/dop_overload_implicit.astra \
tests/valids/dop_pipeline.astra \
tests/valids/dop_struct_methods.astra \
tests/valids/dop_type_inference.astra \
examples/extra_tasks/extra_level1.astra \
examples/extra_tasks/extra_level2.astra \
examples/extra_tasks/extra_level3.astra

do
    name=$(basename "$f" .astra)
    echo "BUILD AND RUN DOP: $f"
    ./build/src/astra "$f" -o "build/test_bin/$name" || exit 1
    "./build/test_bin/$name" || exit 1
    echo
done
```

## Проверка генерации asm для всех valid-файлов

Эта команда проверяет, что все корректные программы проходят lexer, parser, semantic и codegen:

```bash
mkdir -p build/test_asm

for f in tests/valids/*.astra examples/extra_tasks/*.astra; do
    name=$(basename "$f" .astra)
    echo "CHECK VALID: $f"
    ./build/src/astra "$f" --emit-asm -o "build/test_asm/$name.asm" || exit 1
done
```

Если всё нормально, для каждого файла будет сообщение вида:

```text
Generated asm: build/test_asm/name.asm
```

## Проверка invalid-тестов

Эти программы специально неправильные. Они должны завершаться с ошибкой компиляции.

```bash
mkdir -p build/test_asm

for f in tests/invalids/*.astra tests/invalid_extra_tasks/*.astra; do
    echo "CHECK INVALID: $f"
    ./build/src/astra "$f" --emit-asm -o build/test_asm/invalid.asm
    code=$?
    echo "exit code: $code"

    if [ $code -eq 0 ]; then
        echo "BAD: invalid file compiled successfully: $f"
        exit 1
    fi

    echo
done
```

Примеры ошибок, которые проверяются:

- присваивание в `let`;
- неизвестное имя;
- неправильный тип;
- неправильная сигнатура `main`;
- `break` вне цикла;
- неправильный `return`;
- выход за поле, которого нет;
- незакрытый блочный комментарий;
- рекурсивная структура по значению;
- ошибки в дополнительных заданиях.

## Примеры файлов

Основные тесты лежат в:

```text
tests/valids/
tests/invalids/
```

Дополнительные демонстрационные программы лежат в:

```text
examples/extra_tasks/
tests/valids/dop_*.astra
tests/invalid_extra_tasks/
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

## Важное замечание про warning от linker

При сборке программ может появляться такое предупреждение:

```text
/usr/bin/ld: warning: ... missing .note.GNU-stack section implies executable stack
```

Это предупреждение от линковщика из-за NASM-файла. Оно не означает ошибку компиляции: если после него написано `Compilation successful`, программа собрана нормально и её можно запускать.

## Известные ограничения

- нет полноценного import/export между разными файлами;
- нет отдельной IR-фазы между AST и codegen;
- нет полноценного optimizer/constant folding pipeline;
- `print` не печатает массивы и структуры как целые объекты;
- строки реализованы через runtime/C-строки;
- backend ориентирован на x86-64 NASM и System V AMD64 ABI.
