# grammar.md — лексическая и синтаксическая грамматика языка **Astra**

## 1. Общая идея языка

**Astra** — небольшой статически и строго типизированный компилируемый язык общего назначения.
Язык проектируется так, чтобы полностью покрывать обязательные базовые требования ТЗ:

- отдельные лексический, синтаксический и семантический уровни;
- точка входа `main`;
- переменные, функции, структуры, массивы фиксированного размера, синонимы типов, пространства имён;
- выражения, инструкции, управление потоком;
- встроенные функции `print`, `input`, `exit`, `panic`;
- явные приведения типов;
- верхнеуровневые объявления только декларативного характера.

Дополнительные возможности языка:

1. **Вывод типов для переменных** (`let x = 10;`).
2. **Промежуточное представление (IR) и constant folding** как дополнительная стадия компиляции.

---

## 2. Лексика

### 2.1. Алфавит

Исходный текст языка использует Unicode-символы, однако ключевые слова и операторы
определяются в ASCII.

Для целей лексического анализа используются следующие классы символов:

- латинские буквы: `A..Z`, `a..z`
- цифры: `0..9`
- символ подчёркивания: `_`
- пробельные символы: пробел, табуляция, перевод строки, возврат каретки
- знаки пунктуации и операторы

### 2.2. Комментарии

Поддерживаются **однострочные комментарии**:

```txt
// это комментарий
```

Комментарий начинается с `//` и продолжается до конца строки.
Комментарии игнорируются лексером и не попадают в поток токенов.

### 2.3. Ключевые слова

Следующие слова являются зарезервированными и не могут использоваться как идентификаторы:

```txt
namespace type struct fn let var if else while break continue return
true false as unit print input exit panic
```

### 2.4. Идентификаторы

Идентификатор задаётся правилом:

```ebnf
identifier = ( letter | "_" ) , { letter | digit | "_" } ;
letter     = "A".."Z" | "a".."z" ;
digit      = "0".."9" ;
```

Идентификаторы чувствительны к регистру.

Примеры корректных идентификаторов:

```txt
x
main
Point
my_value
print_value
```

### 2.5. Литералы

#### Целые литералы

```ebnf
int_literal = digit , { digit } ;
```

По умолчанию числовой целый литерал имеет тип `int32`, если иное не требуется контекстом
или если литерал не участвует в явном приведении.

#### Вещественные литералы

```ebnf
float_literal = digit , { digit } , "." , digit , { digit } ;
```

По умолчанию вещественный литерал имеет тип `float64`.

#### Булевы литералы

```ebnf
bool_literal = "true" | "false" ;
```

#### Строковые литералы

```ebnf
string_literal = '"' , { string_char } , '"' ;
```

Допускаются escape-последовательности:

```txt
\n \t \\ \"
```

### 2.6. Операторы и разделители

#### Операторы

```txt
+  -  *  /  %
!  && ||
== != < <= > >=
=  as
.
::
```

#### Разделители

```txt
( ) { } [ ] , ; : ->
```

### 2.7. Пробелы

Пробельные символы допустимы между любыми токенами и не влияют на синтаксис,
кроме разделения лексем.

---

## 3. Структура программы

Программа состоит из последовательности модулей. Каждый исходный файл — отдельный модуль.
В пределах одного файла допускаются только верхнеуровневые объявления.

```ebnf
program         = { top_decl } ;

top_decl        = namespace_decl
                | type_alias_decl
                | struct_decl
                | fn_decl
                ;
```

Инструкции и выражения на верхнем уровне запрещены.

Обязательное требование: в полной программе должна существовать функция

```txt
fn main() -> int32 { ... }
```

или эквивалентное имя встроенного 32-битного целого типа, если реализация использует псевдоним.

---

## 4. Грамматика объявлений

### 4.1. Пространства имён

```ebnf
namespace_decl  = "namespace" , identifier , "{" , { top_decl } , "}" ;
```

Пример:

```txt
namespace Math {
    fn abs(x: int32) -> int32 {
        if (x < 0) { return -x; }
        return x;
    }
}
```

### 4.2. Синонимы типов

```ebnf
type_alias_decl = "type" , identifier , "=" , type_expr , ";" ;
```

Пример:

```txt
type Meters = int32;
```

### 4.3. Структуры

```ebnf
struct_decl     = "struct" , identifier , "{" , { field_decl } , "}" ;
field_decl      = identifier , ":" , type_expr , ";" ;
```

Пример:

```txt
struct Point {
    x: int32;
    y: int32;
}
```

### 4.4. Функции

```ebnf
fn_decl         = "fn" , identifier , "(" , [ param_list ] , ")" ,
                  "->" , type_expr , block ;

param_list      = param , { "," , param } ;
param           = identifier , ":" , type_expr ;
```

Пример:

```txt
fn add(a: int32, b: int32) -> int32 {
    return a + b;
}
```

---

## 5. Типы

### 5.1. Синтаксис типов

```ebnf
type_expr       = simple_type | array_type | qualified_type ;

simple_type     = identifier ;
qualified_type  = identifier , "::" , identifier , { "::" , identifier } ;
array_type      = "[" , type_expr , ";" , int_literal , "]" ;
```

Примеры:

```txt
int32
bool
string
Point
Geometry::Point
[int32; 10]
```

Массив имеет **фиксированный размер**, причём размер является частью типа.

---

## 6. Грамматика инструкций

### 6.1. Блок

```ebnf
block           = "{" , { stmt } , "}" ;
```

Блок создаёт новую область видимости.

### 6.2. Инструкции

```ebnf
stmt            = empty_stmt
                | var_decl_stmt
                | assign_stmt
                | expr_stmt
                | if_stmt
                | while_stmt
                | break_stmt
                | continue_stmt
                | return_stmt
                | block
                ;
```

### 6.3. Пустая инструкция

```ebnf
empty_stmt      = ";" ;
```

### 6.4. Объявление переменной

```ebnf
var_decl_stmt   = immutable_decl | mutable_decl ;

immutable_decl  = "let" , identifier , [ ":" , type_expr ] , "=" , expr , ";" ;
mutable_decl    = "var" , identifier , ":" , type_expr , "=" , expr , ";" ;
```

Правила:
- `let` допускает как явный тип, так и вывод типа по инициализатору;
- `var` требует явного типа и задаёт мутабельную переменную.

Примеры:

```txt
let x = 10;
let y: int32 = 20;
var z: int32 = 30;
```

### 6.5. Присваивание

```ebnf
assign_stmt     = lvalue , "=" , expr , ";" ;
```

```ebnf
lvalue          = identifier
                | field_access_expr
                | index_expr
                ;
```

### 6.6. Условный оператор

```ebnf
if_stmt         = "if" , "(" , expr , ")" , block , [ "else" , ( block | if_stmt ) ] ;
```

### 6.7. Цикл

```ebnf
while_stmt      = "while" , "(" , expr , ")" , block ;
```

### 6.8. Break / Continue / Return

```ebnf
break_stmt      = "break" , ";" ;
continue_stmt   = "continue" , ";" ;
return_stmt     = "return" , [ expr ] , ";" ;
```

### 6.9. Инструкция-выражение

```ebnf
expr_stmt       = expr , ";" ;
```

Её основное назначение — вызовы функций с побочными эффектами.

---

## 7. Грамматика выражений

### 7.1. Общая форма

```ebnf
expr                = logical_or_expr ;
```

Ниже грамматика записана с учётом приоритетов от низкого к высокому.

### 7.2. Логические операторы

```ebnf
logical_or_expr     = logical_and_expr , { "||" , logical_and_expr } ;
logical_and_expr    = equality_expr , { "&&" , equality_expr } ;
```

### 7.3. Сравнения

```ebnf
equality_expr       = relational_expr , { ( "==" | "!=" ) , relational_expr } ;
relational_expr     = additive_expr , { ( "<" | "<=" | ">" | ">=" ) , additive_expr } ;
```

### 7.4. Арифметика

```ebnf
additive_expr       = multiplicative_expr , { ( "+" | "-" ) , multiplicative_expr } ;
multiplicative_expr = cast_expr , { ( "*" | "/" | "%" ) , cast_expr } ;
```

### 7.5. Приведение типов

```ebnf
cast_expr           = unary_expr , { "as" , type_expr } ;
```

Оператор `as` левоассоциативен.

### 7.6. Унарные выражения

```ebnf
unary_expr          = [ unary_op ] , postfix_expr ;
unary_op            = "-" | "!" ;
```

### 7.7. Постфиксные выражения

```ebnf
postfix_expr        = primary_expr , { postfix_suffix } ;

postfix_suffix      = call_suffix
                    | field_suffix
                    | index_suffix
                    | namespace_suffix
                    ;

call_suffix         = "(" , [ argument_list ] , ")" ;
field_suffix        = "." , identifier ;
index_suffix        = "[" , expr , "]" ;
namespace_suffix    = "::" , identifier ;

argument_list       = expr , { "," , expr } ;
```

### 7.8. Первичные выражения

```ebnf
primary_expr        = literal
                    | identifier
                    | qualified_name
                    | "(" , expr , ")"
                    | array_literal
                    | struct_literal
                    ;

qualified_name      = identifier , "::" , identifier , { "::" , identifier } ;
```

### 7.9. Литералы

```ebnf
literal             = int_literal
                    | float_literal
                    | bool_literal
                    | string_literal
                    ;
```

### 7.10. Литерал массива

```ebnf
array_literal       = "[" , [ expr , { "," , expr } ] , "]" ;
```

Пример:

```txt
let xs: [int32; 3] = [1, 2, 3];
```

### 7.11. Литерал структуры

```ebnf
struct_literal      = type_name , "{" , [ field_init_list ] , "}" ;
field_init_list     = field_init , { "," , field_init } ;
field_init          = identifier , ":" , expr ;
type_name           = identifier | qualified_name ;
```

Пример:

```txt
let p = Point { x: 10, y: 20 };
```

---

## 8. Приоритет и ассоциативность операторов

От меньшего приоритета к большему:

1. `||`
2. `&&`
3. `==`, `!=`
4. `<`, `<=`, `>`, `>=`
5. `+`, `-`
6. `*`, `/`, `%`
7. `as`
8. унарные `-`, `!`
9. постфиксные: вызов `()`, индексирование `[]`, доступ к полю `.`, доступ через `::`

Ассоциативность:

- бинарные арифметические и логические операторы — левая;
- `as` — левая;
- унарные операторы — правая;
- постфиксные операции — слева направо.

---

## 9. Порядок вычисления

Порядок вычисления выражений в языке **Astra** — **слева направо**.

Это правило относится к:

- операндам бинарных операторов;
- аргументам функции;
- элементам литерала массива;
- инициализаторам полей литерала структуры.

Пример:

```txt
f(g(), h())
```

Сначала вычисляется `g()`, затем `h()`, затем вызывается `f`.

---

## 10. Встроенные функции

В языке предопределены следующие встроенные функции:

```txt
print(value) -> unit
input() -> string
exit(code: int32) -> unit
panic(message: string) -> unit
```

Они доступны в глобальном пространстве имён и не требуют объявления пользователем.

---

## 11. Ограничения грамматики и дополнительные правила

1. Верхний уровень содержит только объявления.
2. Выражения и инструкции допустимы только внутри тела функции.
3. `main` должна быть функцией без параметров с типом результата `int32`.
4. `let` всегда требует инициализатор.
5. `var` всегда требует инициализатор.
6. Массивный литерал без ожидаемого контекста должен иметь хотя бы один элемент.
7. Пустой литерал структуры запрещён, если структура не имеет полей.
8. Использование ключевых слов как идентификаторов запрещено.

---

## 12. Пример полной программы

```txt
namespace Geometry {
    struct Point {
        x: int32;
        y: int32;
    }

    fn manhattan(p: Point) -> int32 {
        return p.x + p.y;
    }
}

type Count = int32;

fn sum(xs: [int32; 3]) -> int32 {
    var i: int32 = 0;
    var acc: int32 = 0;

    while (i < 3) {
        acc = acc + xs[i];
        i = i + 1;
    }

    return acc;
}

fn main() -> int32 {
    let arr: [int32; 3] = [1, 2, 3];
    let p = Geometry::Point { x: 10, y: 20 };

    print(sum(arr));
    print(Geometry::manhattan(p));

    return 0;
}
```
