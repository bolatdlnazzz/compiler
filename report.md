# Отчёт: компилятор языка Astra

## 1. Общая информация о проекте

**Astra** — учебный компилируемый язык программирования со статической типизацией. Проект реализован на **C++23** и построен как полный pipeline компилятора:

```text
исходный файл .astra
        ↓
Lexer        → поток токенов
        ↓
Parser       → AST
        ↓
Semantic     → проверка имён, типов и корректности программы
        ↓
Codegen      → x86-64 NASM assembly
        ↓
nasm + cc    → исполняемый файл
```

Цель проекта — реализовать собственный язык программирования и компилятор для него, покрывающий базовые требования ТЗ: лексический анализ, синтаксический анализ, семантическую проверку, генерацию кода, базовые типы, функции, структуры, массивы, области видимости, встроенные функции и обработку ошибок.

---

## 2. Структура проекта

```text
compiler/
├── src/              — исходный код компилятора
├── specs/            — спецификация языка
├── tests/            — valid/invalid тесты
├── examples/         — демонстрационные примеры
├── README.md         — инструкция по запуску
├── report.md         — отчёт
└── CMakeLists.txt    — сборка проекта
```

Основные файлы в `src/`:

```text
lexer.h / lexer.cpp        — лексический анализатор
parser.h / parser.cpp      — синтаксический анализатор
ast.h                      — узлы AST
ast_dump.h / ast_dump.cpp  — вывод AST
semantic.h / semantic.cpp  — семантический анализатор
codegen.h / codegen.cpp    — генератор x86-64 NASM
runtime.c                  — runtime-функции языка
main.cpp                   — CLI и запуск фаз компиляции
```

---

## 3. Архитектура компилятора

Компилятор сделан как последовательность отдельных фаз. Каждая фаза находится в своём модуле и не смешивается с другими фазами.

### 3.1. Lexer

Лексер читает исходный текст и превращает его в поток токенов. Каждый токен содержит тип, лексему и позицию в файле. Лексер распознаёт ключевые слова, идентификаторы, литералы, операторы, разделители, строки, символы, однострочные и блочные комментарии.

Для проверки лексера используется:

```bash
./build/src/astra tests/valids/hello.astra --dump-tokens
```

### 3.2. Parser

Парсер получает токены и строит AST. Для объявлений и инструкций используется рекурсивный спуск, а для выражений — Pratt parser. Это позволяет правильно разбирать приоритеты операторов, например `2 + 3 * 4` как `2 + (3 * 4)`.

Для проверки AST используется:

```bash
./build/src/astra tests/valids/hello.astra --dump-ast
```

### 3.3. Semantic Analyzer

Семантический анализатор проверяет имена, области видимости, типы выражений, корректность присваиваний, вызовы функций, `return`, `break`, `continue`, структуры, массивы, поля, builtin-функции и дополнительные возможности вроде overload resolution, default/named arguments, access control и module namespace.

### 3.4. Codegen

Codegen получает AST после семантического анализа и генерирует x86-64 NASM assembly. Отдельной IR-фазы нет: используется упрощённый учебный pipeline `AST → NASM → executable`.

Для генерации только assembly используется:

```bash
./build/src/astra tests/valids/math.astra --emit-asm -o build/math.asm
```

### 3.5. Runtime

Runtime находится в `src/runtime.c` и содержит C-функции для печати, строк, `input`, `exit`, `panic`, `assert`, ошибок деления на ноль и выхода за границы массива.

---

## 4. Базовые возможности языка

Реализованы:

* точка входа `main`;
* переменные `let` и `var`;
* встроенные типы `int8/int16/int32/int64`, `uint8/uint16/uint32/uint64`, `float32/float64`, `bool`, `char`, `string`, `unit`;
* функции;
* блоки, `if/else`, `while`, `break`, `continue`, `return`;
* арифметические, логические операции и сравнения;
* массивы фиксированного размера;
* структуры;
* type alias;
* namespace;
* builtin-функции `print`, `input`, `len`, `exit`, `panic`, `assert`;
* runtime-проверки ошибок.

---

## 5. Дополнительные задания и команды запуска

Перед запуском доп. заданий нужно собрать проект:

```bash
cmake -S . -B build
cmake --build build -j
mkdir -p build/test_asm build/test_bin
```

---

### A.1.7 — автоматический вывод типов

**Что реализовано:** вывод типа для `let`-переменных без явного типа и вывод типа результата функции, если у функции не указан `-> Type`.

**Как показать генерацию asm:**

```bash
./build/src/astra tests/valids/dop_type_inference.astra --emit-asm -o build/test_asm/dop_type_inference.asm
```

**Как собрать и запустить:**

```bash
./build/src/astra tests/valids/dop_type_inference.astra -o build/test_bin/dop_type_inference
./build/test_bin/dop_type_inference
```

**Как показать AST:**

```bash
./build/src/astra tests/valids/dop_type_inference.astra --dump-ast
```

---

### A.1.10 — if-выражение

**Что реализовано:** `if` может использоваться как выражение и возвращать значение.

Пример:

```astra
let x = if (flag) { 100 } else { 0 };
```

**Как показать AST:**

```bash
./build/src/astra tests/valids/dop_if_expr.astra --dump-ast
```

В AST должен быть узел `IfExpr`.

**Как показать генерацию asm:**

```bash
./build/src/astra tests/valids/dop_if_expr.astra --emit-asm -o build/test_asm/dop_if_expr.asm
```

**Как собрать и запустить:**

```bash
./build/src/astra tests/valids/dop_if_expr.astra -o build/test_bin/dop_if_expr
./build/test_bin/dop_if_expr
```

**Как показать invalid-тест:**

```bash
./build/src/astra tests/invalid_extra_tasks/invalid_if_expr_branch_types.astra --emit-asm -o build/test_asm/invalid_if_expr_branch_types.asm
```

---

### A.1.11 — конвейерный оператор `|>`

**Что реализовано:** оператор `|>` передаёт результат выражения слева первым аргументом в функцию справа.

Пример:

```astra
5 |> inc |> double
```

Семантически это превращается в:

```astra
double(inc(5))
```

**Как показать токен `|>`:**

```bash
./build/src/astra tests/valids/dop_pipeline.astra --dump-tokens | grep "|>"
```

**Как показать AST:**

```bash
./build/src/astra tests/valids/dop_pipeline.astra --dump-ast
```

В AST pipeline уже должен быть превращён в обычный `Call`.

**Как показать генерацию asm:**

```bash
./build/src/astra tests/valids/dop_pipeline.astra --emit-asm -o build/test_asm/dop_pipeline.asm
```

**Как собрать и запустить:**

```bash
./build/src/astra tests/valids/dop_pipeline.astra -o build/test_bin/dop_pipeline
./build/test_bin/dop_pipeline
```

---

### A.1.13 — метафункции `sizeof`, `typeid`, `typeof`

**Что реализовано:** простые compile-time метафункции.

```astra
sizeof(Type)   // размер типа в байтах
typeid(expr)   // строка с типом выражения
typeof(expr)   // строка с типом выражения
```

**Как показать токены:**

```bash
./build/src/astra tests/valids/dop_meta.astra --dump-tokens | grep -E "sizeof|typeid|typeof"
```

**Как показать AST:**

```bash
./build/src/astra tests/valids/dop_meta.astra --dump-ast
```

В AST должны быть узлы `SizeOf`, `TypeId`, `TypeOf`.

**Как показать генерацию asm:**

```bash
./build/src/astra tests/valids/dop_meta.astra --emit-asm -o build/test_asm/dop_meta.asm
```

**Как собрать и запустить:**

```bash
./build/src/astra tests/valids/dop_meta.astra -o build/test_bin/dop_meta
./build/test_bin/dop_meta
```

---

### A.1.15 — блочные комментарии

**Что реализовано:** комментарии вида `/* ... */`, включая вложенные блочные комментарии.

**Как показать valid-тест:**

```bash
./build/src/astra tests/valids/dop_block_comment.astra --emit-asm -o build/test_asm/dop_block_comment.asm
./build/src/astra tests/valids/dop_block_comment.astra -o build/test_bin/dop_block_comment
./build/test_bin/dop_block_comment
```

**Как показать invalid-тест незакрытого комментария:**

```bash
./build/src/astra tests/invalids/unclosed_block_comment.astra --emit-asm -o build/test_asm/unclosed_block_comment.asm
```

---

### A.2.8 — перегрузка функций

**Что реализовано:** несколько функций могут иметь одно имя, если отличаются типами параметров.

Пример:

```astra
fn show(x: int32) -> int32 { ... }
fn show(x: string) -> int32 { ... }
```

**Как показать генерацию asm:**

```bash
./build/src/astra tests/valids/dop_overload.astra --emit-asm -o build/test_asm/dop_overload.asm
```

**Как собрать и запустить:**

```bash
./build/src/astra tests/valids/dop_overload.astra -o build/test_bin/dop_overload
./build/test_bin/dop_overload
```

---

### A.2.9 — параметры по умолчанию и именованные аргументы

**Что реализовано:** параметры могут иметь default-значения, а вызовы функций могут передавать аргументы по имени.

Пример:

```astra
fn add(a: int32, b: int32 = 10) -> int32 {
    return a + b;
}

add(a = 5, b = 7);
```

**Как показать valid-тест:**

```bash
./build/src/astra tests/valids/dop_default_named_args.astra --emit-asm -o build/test_asm/dop_default_named_args.asm
./build/src/astra tests/valids/dop_default_named_args.astra -o build/test_bin/dop_default_named_args
./build/test_bin/dop_default_named_args
```

**Как показать invalid-тест неизвестного named argument:**

```bash
./build/src/astra tests/invalid_extra_tasks/invalid_named_argument.astra --emit-asm -o build/test_asm/invalid_named_argument.asm
```

**Как показать invalid-тест неправильного порядка default-параметров:**

```bash
./build/src/astra tests/invalid_extra_tasks/invalid_default_param_order.astra --emit-asm -o build/test_asm/invalid_default_param_order.asm
```

---

### A.2.12 — методы структур и `public/private`

**Что реализовано:** методы структур, `self`-параметр, модификаторы доступа `public/private` для полей и методов.

Пример метода:

```astra
fn Point.sum(self: Point) -> int32 {
    return self.x + self.y;
}
```

**Как показать методы структур:**

```bash
./build/src/astra tests/valids/dop_struct_methods.astra --emit-asm -o build/test_asm/dop_struct_methods.asm
./build/src/astra tests/valids/dop_struct_methods.astra -o build/test_bin/dop_struct_methods
./build/test_bin/dop_struct_methods
```

**Как показать `public/private`:**

```bash
./build/src/astra tests/valids/dop_struct_visibility.astra --emit-asm -o build/test_asm/dop_struct_visibility.asm
./build/src/astra tests/valids/dop_struct_visibility.astra -o build/test_bin/dop_struct_visibility
./build/test_bin/dop_struct_visibility
```

**Как показать invalid-тест неправильного receiver:**

```bash
./build/src/astra tests/invalid_extra_tasks/invalid_method_receiver.astra --emit-asm -o build/test_asm/invalid_method_receiver.asm
```

**Как показать invalid-тест private-метода:**

```bash
./build/src/astra tests/invalid_extra_tasks/private_method_access.astra --emit-asm -o build/test_asm/private_method_access.asm
```

**Как показать invalid-тест private-поля:**

```bash
./build/src/astra tests/invalid_extra_tasks/private_field_access.astra --emit-asm -o build/test_asm/private_field_access.asm
```

---

### A.2.20 — модули

**Что реализовано:** директива `module`, которая задаёт модульную область имён для top-level объявлений файла.

Пример:

```astra
module demo;
```

**Как показать explicit module:**

```bash
./build/src/astra tests/valids/dop_module.astra --emit-asm -o build/test_asm/dop_module.asm
./build/src/astra tests/valids/dop_module.astra -o build/test_bin/dop_module
./build/test_bin/dop_module
```

Если в проекте файл называется `dop_module_explicit.astra`, команда такая:

```bash
./build/src/astra tests/valids/dop_module_explicit.astra --emit-asm -o build/test_asm/dop_module_explicit.asm
./build/src/astra tests/valids/dop_module_explicit.astra -o build/test_bin/dop_module_explicit
./build/test_bin/dop_module_explicit
```

**Как показать implicit module из имени файла:**

```bash
./build/src/astra tests/valids/math.astra --dump-ast
```

В AST должен быть виден модуль, полученный из имени файла.

---

### A.3.1 — перегрузка с неявными приведениями

**Что реализовано:** перегрузка функций учитывает не только точные совпадения типов, но и допустимые неявные приведения.

Пример:

```astra
fn f(x: int64) -> int64 { ... }
f(5); // 5 имеет int32, но может быть приведён к int64
```

**Как показать valid-тест:**

```bash
./build/src/astra tests/valids/dop_overload_implicit.astra --emit-asm -o build/test_asm/dop_overload_implicit.asm
./build/src/astra tests/valids/dop_overload_implicit.astra -o build/test_bin/dop_overload_implicit
./build/test_bin/dop_overload_implicit
```

**Как показать invalid-тест неоднозначности:**

```bash
./build/src/astra tests/invalid_extra_tasks/invalid_ambiguous_overload.astra --emit-asm -o build/test_asm/invalid_ambiguous_overload.asm
```

---

### B.1.4 — Pratt parser выражений

**Что реализовано:** выражения разбираются через Pratt parser с таблицей приоритетов операторов.

Пример:

```astra
2 + 3 * 4
```

Должно разбираться как:

```text
2 + (3 * 4)
```

**Как показать AST с правильным приоритетом:**

```bash
./build/src/astra tests/valids/math.astra --dump-ast
```

В AST нужно показать, что `*` находится глубже, чем `+`.

**Как собрать и запустить math-тест:**

```bash
./build/src/astra tests/valids/math.astra -o build/test_bin/math
./build/test_bin/math
```

---

### B.1.8 — CLI-режимы `--dump-tokens`, `--dump-ast`, `--emit-asm`

**Что реализовано:** дополнительные режимы командной строки для демонстрации фаз компилятора.

**Показать токены после lexer:**

```bash
./build/src/astra tests/valids/hello.astra --dump-tokens
```

**Показать AST после parser:**

```bash
./build/src/astra tests/valids/hello.astra --dump-ast
```

**Показать генерацию asm после codegen:**

```bash
./build/src/astra tests/valids/hello.astra --emit-asm -o build/test_asm/hello.asm
cat build/test_asm/hello.asm
```

---

### Дополнительная реализованная возможность — битовые операции

**Что реализовано:** битовые операции для целочисленных типов.

```text
~  &  |  ^  <<  >>
```

**Как показать генерацию asm:**

```bash
./build/src/astra tests/valids/dop_bitwise.astra --emit-asm -o build/test_asm/dop_bitwise.asm
```

**Как собрать и запустить:**

```bash
./build/src/astra tests/valids/dop_bitwise.astra -o build/test_bin/dop_bitwise
./build/test_bin/dop_bitwise
```

---

## 6. Быстрый запуск всех valid-допов

```bash
mkdir -p build/test_bin build/test_asm

for f in \
  tests/valids/dop_type_inference.astra \
  tests/valids/dop_if_expr.astra \
  tests/valids/dop_pipeline.astra \
  tests/valids/dop_meta.astra \
  tests/valids/dop_block_comment.astra \
  tests/valids/dop_overload.astra \
  tests/valids/dop_default_named_args.astra \
  tests/valids/dop_struct_methods.astra \
  tests/valids/dop_struct_visibility.astra \
  tests/valids/dop_module.astra \
  tests/valids/dop_overload_implicit.astra \
  tests/valids/dop_bitwise.astra; do
    name=$(basename "$f" .astra)
    echo "BUILD/RUN: $f"
    ./build/src/astra "$f" --emit-asm -o "build/test_asm/$name.asm" || exit 1
    ./build/src/astra "$f" -o "build/test_bin/$name" || exit 1
    "build/test_bin/$name" || exit 1
    echo
 done
```

Если файл `tests/valids/dop_module.astra` в проекте называется `tests/valids/dop_module_explicit.astra`, в цикле нужно заменить имя файла.

---

## 7. Быстрый запуск invalid-допов

```bash
mkdir -p build/test_asm

for f in \
  tests/invalids/unclosed_block_comment.astra \
  tests/invalid_extra_tasks/invalid_if_expr_branch_types.astra \
  tests/invalid_extra_tasks/invalid_named_argument.astra \
  tests/invalid_extra_tasks/invalid_default_param_order.astra \
  tests/invalid_extra_tasks/invalid_method_receiver.astra \
  tests/invalid_extra_tasks/private_method_access.astra \
  tests/invalid_extra_tasks/private_field_access.astra \
  tests/invalid_extra_tasks/invalid_ambiguous_overload.astra; do
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

---

## 8. Запуск основных программ

```bash
mkdir -p build/test_bin

./build/src/astra tests/valids/hello.astra -o build/test_bin/hello
./build/test_bin/hello

./build/src/astra tests/valids/math.astra -o build/test_bin/math
./build/test_bin/math

./build/src/astra tests/valids/array.astra -o build/test_bin/array
./build/test_bin/array

./build/src/astra tests/valids/point.astra -o build/test_bin/point
./build/test_bin/point

./build/src/astra tests/valids/functions.astra -o build/test_bin/functions
./build/test_bin/functions

./build/src/astra tests/valids/branch.astra -o build/test_bin/branch
./build/test_bin/branch

./build/src/astra tests/valids/loop.astra -o build/test_bin/loop
./build/test_bin/loop

./build/src/astra tests/valids/cast.astra -o build/test_bin/cast
./build/test_bin/cast
```

---

## 9. Проверка всех invalid-тестов

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

Некорректные программы должны выводить сообщение вида:

```text
file:line:column: error: message
```

и завершаться с ненулевым кодом.

---

## 10. Известные ограничения

1. Нет полноценного `import/export` между разными файлами.
2. `module` реализует модульное пространство имён внутри файла, но не полноценную систему пакетов.
3. Нет отдельной IR-фазы между AST и NASM.
4. Нет отдельного оптимизатора.
5. `print` не печатает массивы и структуры.
6. Строки реализованы через runtime/C-строки.
7. Нет generics.
8. Нет пользовательской перегрузки операторов.
9. `public/private` реализованы для полей и методов структур, но нет отдельной системы экспортов между файлами.
10. Количество аргументов ограничено используемыми регистрами ABI: до 6 integer/string/aggregate-аргументов и до 8 float-аргументов.

---

## 11. Итог

В проекте реализован учебный компилятор языка Astra с полным pipeline:

```text
Lexer → Parser → Semantic Analyzer → x86-64 NASM Codegen → executable
```

Базовые требования покрыты: лексер, парсер, AST, семантический анализ, генерация кода, CLI, `main`, переменные, функции, структуры, массивы, type alias, namespace, базовые типы, выражения, управление потоком, builtin-функции и runtime-ошибки.

Также реализованы дополнительные задания:

```text
A.1.7  — автоматический вывод типов
A.1.10 — if-выражение
A.1.11 — конвейерный оператор |>
A.1.13 — sizeof/typeid/typeof
A.1.15 — блочные комментарии
A.2.8  — перегрузка функций
A.2.9  — default-параметры и named arguments
A.2.12 — методы структур и public/private
A.2.20 — модули
A.3.1  — перегрузка с неявными приведениями
B.1.4  — Pratt parser
B.1.8  — CLI dump/emit-asm
```

Рядом с каждым дополнительным заданием в отчёте указаны команды для генерации assembly, запуска valid-теста и проверки invalid-теста, если он есть.
