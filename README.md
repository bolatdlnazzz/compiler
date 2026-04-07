# ZLang Compiler

Семестровый проект — разработка языка программирования **ZLang** и его компилятора.

## Возможности

| Требование | Реализация |
|---|---|
| Проектирование языка | ZLang — статически типизированный процедурный язык |
| Формальная грамматика | EBNF-грамматика в [`grammar.md`](grammar.md) |
| Лексический анализатор | `src/lexer.py` — токенизация исходного кода |
| Синтаксический анализатор | `src/parser.py` — рекурсивный спуск, строит AST |
| Семантические проверки | `src/semantic.py` — проверка типов, области видимости |
| Вывод типов | `src/semantic.py` — `let x = expr` выводит тип из выражения |
| Генерация кода (→ C++) | `src/codegen.py` — транспиляция в C++17 |

---

## Структура проекта

```
compiler/
├── src/
│   ├── __init__.py
│   ├── lexer.py        # Лексический анализатор
│   ├── ast_nodes.py    # Узлы абстрактного синтаксического дерева
│   ├── parser.py       # Синтаксический анализатор (рекурсивный спуск)
│   ├── semantic.py     # Семантический анализ и вывод типов
│   ├── codegen.py      # Генерация кода C++
│   └── main.py         # Точка входа (CLI)
├── tests/
│   ├── test_lexer.py
│   ├── test_parser.py
│   ├── test_semantic.py
│   └── test_codegen.py
├── examples/
│   ├── hello.zlang
│   ├── fibonacci.zlang
│   ├── factorial.zlang
│   └── types.zlang
├── grammar.md          # Формальная грамматика ZLang (EBNF)
└── README.md
```

---

## Язык ZLang

### Типы данных

| Тип ZLang | Описание | C++ эквивалент |
|---|---|---|
| `int` | Целое число | `int` |
| `float` | Число с плавающей точкой | `double` |
| `bool` | Логический (`true`/`false`) | `bool` |
| `string` | Строка | `std::string` |
| `void` | Отсутствие значения (только как тип возврата) | `void` |

### Пример программы

```zlang
# Пример программы на ZLang

func factorial(n: int) -> int {
    if n <= 1 {
        return 1
    }
    return n * factorial(n - 1)
}

func greet(name: string) -> void {
    print("Hello, " + name + "!")
}

func main() -> void {
    # Вывод типа: z имеет тип int
    let z = factorial(10)
    print(z)

    greet("World")

    # Цикл while
    let i: int = 0
    while i < 5 {
        print(i)
        i = i + 1
    }

    # Цикл for по диапазону
    for j in range(0, 10) {
        print(j)
    }
}
```

### Вывод типов

Когда тип переменной не указан явно, компилятор выводит его автоматически:

```zlang
let x = 42          # вывод: int
let y = 3.14        # вывод: float
let ok = true       # вывод: bool
let s = "hello"     # вывод: string
let r = factorial(5) # вывод: int (из типа возврата функции)
```

---

## Использование

### Требования

- Python 3.9+
- g++ (для компиляции сгенерированного C++)

### Запуск программы

```bash
# Компилировать и выполнить
python -m src.main examples/hello.zlang

# Посмотреть сгенерированный C++
python -m src.main examples/fibonacci.zlang --emit-cpp

# Вывести AST
python -m src.main examples/types.zlang --emit-ast

# Сохранить C++ в файл
python -m src.main examples/factorial.zlang -o output.cpp
```

### Запуск тестов

```bash
python -m pytest tests/ -v
```

---

## Архитектура компилятора

```
Исходный код ZLang
       │
       ▼
 ┌──────────┐  токены  ┌─────────┐   AST   ┌────────────────────┐
 │  Lexer   │─────────►│ Parser  │────────►│ SemanticAnalyzer   │
 └──────────┘          └─────────┘         └────────────────────┘
                                                     │ аннотированное AST
                                                     ▼
                                           ┌──────────────────┐
                                           │  CodeGenerator   │
                                           └──────────────────┘
                                                     │ исходный C++
                                                     ▼
                                               g++ / clang++
                                                     │ бинарный файл
                                                     ▼
                                                 выполнение
```

### Фазы компиляции

1. **Лексический анализ** (`lexer.py`)  
   Исходный текст → последовательность токенов.  
   Обрабатывает числовые/строковые литералы, ключевые слова, операторы,
   однострочные комментарии (`#`).

2. **Синтаксический анализ** (`parser.py`)  
   Токены → Абстрактное синтаксическое дерево (AST).  
   Рекурсивный спуск по EBNF-грамматике, описанной в `grammar.md`.

3. **Семантический анализ** (`semantic.py`)  
   Два прохода:
   - **Проход 1**: сбор сигнатур всех функций (позволяет взаимные/опережающие вызовы).
   - **Проход 2**: обход тела каждой функции — проверка типов операндов,
     вывод типов переменных, контроль области видимости, проверка типов возврата.

4. **Генерация кода** (`codegen.py`)  
   Аннотированное AST → исходный C++17.  
   Каждый узел AST отображается в соответствующую конструкцию C++.

---

## Примеры сгенерированного C++

Исходный ZLang:
```zlang
func add(a: int, b: int) -> int {
    return a + b
}

func main() -> void {
    let result = add(3, 4)
    print(result)
    for i in range(0, 5) {
        print(i)
    }
}
```

Сгенерированный C++:
```cpp
// Generated by ZLang compiler
#include <iostream>
#include <string>

int add(int a, int b) {
    return (a + b);
}

int main() {
    int result = add(3, 4);
    std::cout << result << '\n';
    for (int i = 0; i < 5; ++i) {
        std::cout << i << '\n';
    }
    return 0;
}
```
