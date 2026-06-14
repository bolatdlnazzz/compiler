# grammar.md — актуальная лексическая и синтаксическая грамматика языка **Astra**

## 1. Общая идея языка

**Astra** — небольшой статически типизированный компилируемый язык общего назначения.
Текущая реализация компилятора проходит классический pipeline:

```text
source .astra
  -> Lexer
  -> Parser
  -> AST
  -> Semantic Analyzer
  -> x86-64 NASM Codegen
  -> nasm + cc + runtime.c
  -> executable
```

Текущая версия языка поддерживает базовые конструкции и ряд дополнительных возможностей:

- функции, включая перегрузку;
- необязательный явный тип результата функции с выводом по `return`;
- переменные `let` и `var`;
- вывод типа для `let` по инициализатору;
- структуры и методы структур;
- массивы фиксированного размера;
- пространства имён `namespace`;
- директиву модуля `module` в начале файла;
- синонимы типов `type`;
- арифметические, логические, сравнительные и битовые операции;
- конвейерный оператор `|>`;
- `if` как инструкция и как выражение;
- `while`, `break`, `continue`, `return`;
- явные приведения `as`;
- встроенные функции `print`, `input`, `len`, `exit`, `panic`, `assert`;
- простые метафункции `sizeof`, `typeid`, `typeof`.

---

## 2. Лексика

### 2.1. Алфавит

Ключевые слова, операторы и идентификаторы задаются ASCII-символами.
Строки могут содержать произвольные байты UTF-8, но лексер не выполняет полноценную Unicode-нормализацию идентификаторов.

Классы символов:

```ebnf
letter = "A".."Z" | "a".."z" ;
digit  = "0".."9" ;
hex    = digit | "A".."F" | "a".."f" ;
ident_start = letter | "_" ;
ident_part  = letter | digit | "_" ;
```

### 2.2. Комментарии

Поддерживаются однострочные комментарии:

```astra
// comment
```

и блочные комментарии с вложенностью:

```astra
/* outer
   /* inner */
   outer continues
*/
```

Комментарии игнорируются лексером и не попадают в поток токенов.

### 2.3. Ключевые слова

```text
module namespace type struct fn let var
if else while break continue return
true false as unit
print input len exit panic assert
sizeof typeid typeof
```

`true` и `false` лексически распознаются как булевы литералы.

### 2.4. Идентификаторы

```ebnf
identifier = ident_start , { ident_part } ;
```

Идентификаторы чувствительны к регистру.

Примеры:

```astra
x
main
Point
my_value
print_value
```

### 2.5. Литералы

#### Целые литералы

```ebnf
int_literal = decimal_int | hex_int | binary_int ;
decimal_int = digit , { digit } ;
hex_int     = "0" , ("x" | "X") , hex , { hex } ;
binary_int  = "0" , ("b" | "B") , ("0" | "1") , { "0" | "1" } ;
```

Примеры:

```astra
42
0x2A
0b101010
```

#### Вещественные литералы

```ebnf
float_literal = decimal_float | exponent_float | special_float ;
decimal_float = digit , { digit } , "." , digit , { digit } , [ exponent ] ;
exponent_float = digit , { digit } , exponent ;
exponent = ("e" | "E") , [ "+" | "-" ] , digit , { digit } ;
special_float = "inf" | "NaN" | "nan" ;
```

Примеры:

```astra
3.14
1e3
1.5e-2
inf
NaN
```

#### Булевы литералы

```ebnf
bool_literal = "true" | "false" ;
```

#### Символьные литералы

```ebnf
char_literal = "'" , char_body , "'" ;
```

Поддерживаемые escape-последовательности:

```text
\n  \t  \\  \'  \0
```

Примеры:

```astra
'A'
'\n'
'\0'
```

#### Строковые литералы

```ebnf
string_literal = '"' , { string_char } , '"' ;
```

Поддерживаемые escape-последовательности:

```text
\n  \t  \\  \"
```

Пример:

```astra
"hello\n"
```

### 2.6. Операторы и разделители

Операторы:

```text
+  -  *  /  %
!  ~
&& ||
&  |  ^  << >>
== != < <= > >=
=  as
.  ::  |>
```

Разделители:

```text
( ) { } [ ] , ; : ->
```

Составные присваивания вида `+=`, `-=`, `&=` в текущей реализации не заявляются как поддерживаемые.

---

## 3. Структура программы

```ebnf
program = [ module_decl ] , { top_decl } ;

module_decl = "module" , module_name , ";" ;
module_name = identifier , { "::" , identifier } ;

top_decl = namespace_decl
         | type_alias_decl
         | struct_decl
         | fn_decl
         ;
```

`module` допустим только в начале файла. Текущий CLI компилирует один исходный файл за запуск; `module` создаёт модульную область имён, но не является импортом другого файла.

На верхнем уровне разрешены только объявления.

Пример:

```astra
module Demo::Math;

fn main() -> int32 {
    return 0;
}
```

Обязательная точка входа:

```astra
fn main() -> int32 { ... }
```

Текущая реализация также принимает `main() -> int64` как допустимую сигнатуру точки входа.

---

## 4. Объявления

### 4.1. Пространства имён

```ebnf
namespace_decl = "namespace" , identifier , "{" , { top_decl } , "}" ;
```

Пример:

```astra
namespace Math {
    fn abs(x: int32) -> int32 {
        if (x < 0) { return -x; }
        return x;
    }
}
```

Доступ к вложенным объявлениям выполняется через `::`:

```astra
Math::abs(10)
```

### 4.2. Type alias

```ebnf
type_alias_decl = "type" , identifier , "=" , type_expr , ";" ;
```

Пример:

```astra
type Count = int32;
```

### 4.3. Структуры

```ebnf
struct_decl = "struct" , identifier , "{" , { field_decl } , "}" ;
field_decl  = identifier , ":" , type_expr , ";" ;
```

Пример:

```astra
struct Point {
    x: int32;
    y: int32;
}
```

### 4.4. Функции

```ebnf
fn_decl = "fn" , function_name , "(" , [ param_list ] , ")" ,
          [ "->" , type_expr ] , block ;

function_name = identifier | identifier , "." , identifier ;

param_list = param , { "," , param } ;
param      = identifier , ":" , type_expr , [ "=" , expr ] ;
```

Если тип результата после `->` отсутствует, он выводится по `return` внутри тела функции.
Если в функции без явного типа результата нет `return expr`, результат считается `unit`.

Примеры:

```astra
fn add(a: int32, b: int32) -> int32 {
    return a + b;
}

fn inferred(a: int32, b: int32) {
    return a + b; // int32
}
```

Параметры могут иметь значения по умолчанию:

```astra
fn power(base: int32, exp: int32 = 2) -> int32 {
    return base * exp;
}
```

После параметра со значением по умолчанию не должен идти параметр без значения по умолчанию.

### 4.5. Методы структур

Метод объявляется как функция с именем вида `Type.method`.
Первый параметр должен быть параметром получателя, обычно `self`:

```astra
struct Point {
    x: int32;
    y: int32;
}

fn Point.sum(self: Point) -> int32 {
    return self.x + self.y;
}
```

Вызов метода:

```astra
let p = Point { x: 10, y: 20 };
print(p.sum());
```

Семантически `p.sum()` преобразуется в вызов функции метода с `p` как первым аргументом.

---

## 5. Типы

```ebnf
type_expr = simple_type | qualified_type | array_type ;

simple_type    = identifier ;
qualified_type = identifier , "::" , identifier , { "::" , identifier } ;
array_type     = "[" , type_expr , ";" , int_literal , "]" ;
```

Встроенные типы:

```text
int8 int16 int32 int64
uint8 uint16 uint32 uint64
float32 float64
bool char string unit
```

Примеры:

```astra
int32
char
string
Point
Geometry::Point
[int32; 10]
```

Размер массива является частью типа.

---

## 6. Инструкции

### 6.1. Блок

```ebnf
block = "{" , { stmt } , "}" ;
```

### 6.2. Инструкции

```ebnf
stmt = empty_stmt
     | let_stmt
     | var_stmt
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

### 6.3. Переменные

```ebnf
let_stmt = "let" , identifier , [ ":" , type_expr ] , "=" , expr , ";" ;
var_stmt = "var" , identifier , ":" , type_expr , "=" , expr , ";" ;
```

`let` может иметь явный тип или выводить тип по инициализатору.
`var` требует явного типа.

Примеры:

```astra
let x = 10;
let y: int32 = 20;
var z: int32 = 30;
```

### 6.4. Присваивание

```ebnf
assign_stmt = lvalue , "=" , expr , ";" ;

lvalue = identifier
       | field_access_expr
       | index_expr
       ;
```

### 6.5. Условная инструкция

```ebnf
if_stmt = "if" , "(" , expr , ")" , block , [ "else" , ( block | if_stmt ) ] ;
```

### 6.6. Цикл

```ebnf
while_stmt = "while" , "(" , expr , ")" , block ;
```

### 6.7. Break / Continue / Return

```ebnf
break_stmt    = "break" , ";" ;
continue_stmt = "continue" , ";" ;
return_stmt   = "return" , [ expr ] , ";" ;
```

### 6.8. Инструкция-выражение

```ebnf
expr_stmt = expr , ";" ;
```

---

## 7. Выражения

Текущий parser использует Pratt-разбор выражений.
Ниже операторы перечислены от меньшего приоритета к большему.
Все инфиксные операторы левоассоциативны.

| Приоритет | Операторы |
|---:|---|
| 1 | `|>` |
| 2 | `||` |
| 3 | `&&` |
| 4 | `|` |
| 5 | `^` |
| 6 | `&` |
| 7 | `==`, `!=` |
| 8 | `<`, `<=`, `>`, `>=` |
| 9 | `<<`, `>>` |
| 10 | `+`, `-`, `as` |
| 11 | `*`, `/`, `%` |
| postfix | `()`, `.`, `[]`, `::` |
| unary | `-`, `!`, `~` |

### 7.1. Общая форма

```ebnf
expr = precedence_expr ;
```

### 7.2. Унарные выражения

```ebnf
unary_expr = unary_op , unary_expr | postfix_expr ;
unary_op   = "-" | "!" | "~" ;
```

### 7.3. Постфиксные выражения

```ebnf
postfix_expr = primary_expr , { postfix_suffix } ;

postfix_suffix = call_suffix
               | field_suffix
               | index_suffix
               | namespace_suffix
               ;

call_suffix      = "(" , [ argument_list ] , ")" ;
field_suffix     = "." , identifier ;
index_suffix     = "[" , expr , "]" ;
namespace_suffix = "::" , identifier ;
```

### 7.4. Аргументы вызова

```ebnf
argument_list = argument , { "," , argument } ;
argument      = [ identifier , "=" ] , expr ;
```

Примеры:

```astra
foo(1, 2)
foo(a = 1, b = 2)
foo(1, b = 2)
```

### 7.5. Первичные выражения

```ebnf
primary_expr = literal
             | identifier_or_qualified_name
             | "(" , expr , ")"
             | array_literal
             | struct_literal
             | if_expr
             | sizeof_expr
             | typeid_expr
             | typeof_expr
             ;
```

### 7.6. Массивы и структуры

```ebnf
array_literal = "[" , [ expr , { "," , expr } ] , "]" ;

struct_literal = type_name , "{" , [ field_init_list ] , "}" ;
field_init_list = field_init , { "," , field_init } ;
field_init = identifier , ":" , expr ;
type_name = identifier | qualified_name ;
```

Примеры:

```astra
let xs = [1, 2, 3];
let p = Point { x: 10, y: 20 };
```

### 7.7. If-выражение

```ebnf
if_expr = "if" , "(" , expr , ")" , "{" , expr , "}" ,
          "else" , "{" , expr , "}" ;
```

`else` обязателен.

Пример:

```astra
let max = if (a > b) { a } else { b };
```

### 7.8. Конвейерный оператор

```ebnf
pipeline_expr = expr , "|>" , expr ;
```

`x |> f` десугарится как `f(x)`.
Если справа уже вызов, левый операнд вставляется первым аргументом:

```astra
x |> f(10)
```

эквивалентно:

```astra
f(x, 10)
```

### 7.9. Метафункции

```ebnf
sizeof_expr = "sizeof" , "(" , type_expr , ")" ;
typeid_expr = "typeid" , "(" , expr , ")" ;
typeof_expr = "typeof" , "(" , expr , ")" ;
```

Текущая реализация:

- `sizeof(T)` возвращает `int32`;
- `typeid(expr)` возвращает строку с каноническим типом выражения;
- `typeof(expr)` также возвращает строку с каноническим типом выражения.

---

## 8. Встроенные функции

```text
print(value) -> unit
input() -> string
len(value) -> int32
exit(code: int32) -> unit
panic(message: string) -> unit
assert(condition: bool) -> unit
```

Особенности:

- `print` поддерживает числовые типы, `bool`, `char`, `string`;
- `len(string)` возвращает длину C-строки в байтах;
- `len([T; N])` возвращает `N`;
- `assert(false)` завершает программу runtime-ошибкой.

---

## 9. Ограничения текущей грамматики

1. На верхнем уровне разрешены только объявления.
2. `module` разрешён только в начале файла.
3. Полноценный `import/export` между файлами не реализован.
4. `var` всегда требует явный тип.
5. `let` всегда требует инициализатор.
6. Пустой массивный литерал без ожидаемого контекста не поддерживается.
7. Составные присваивания `+=`, `-=`, `&=` и т.п. не входят в текущую грамматику.
8. Методы структур объявляются снаружи структуры через `fn Type.method(...)`, а не внутри тела `struct`.
