# ZLang – Formal Grammar

ZLang is a statically-typed, procedural programming language that compiles to
C++ via source-to-source transpilation.

---

## 1. Lexical grammar (regular language)

```
digit      = '0' | '1' | … | '9' ;
letter     = 'a' | … | 'z' | 'A' | … | 'Z' ;
ident_char = letter | digit | '_' ;

INTEGER  = digit { digit } ;
FLOAT    = digit { digit } '.' digit { digit } ;
BOOL     = 'true' | 'false' ;
STRING   = '"' { char | escape_seq } '"' ;
escape_seq = '\\' ( 'n' | 't' | '\\' | '"' ) ;

IDENT    = letter ident_char* ;          (* not a keyword *)

(* single-line comment – ignored by the lexer *)
COMMENT  = '#' { any_char } newline ;
```

### Reserved keywords

| Keyword  | Purpose                       |
|----------|-------------------------------|
| `func`   | function declaration          |
| `let`    | variable declaration          |
| `if`     | conditional statement         |
| `else`   | alternative branch            |
| `while`  | while loop                    |
| `for`    | range-for loop                |
| `in`     | part of `for … in range(…)`   |
| `range`  | part of `for … in range(…)`   |
| `return` | return from function          |
| `print`  | built-in output               |
| `and`    | logical conjunction           |
| `or`     | logical disjunction           |
| `not`    | logical negation              |
| `int`    | integer type                  |
| `float`  | floating-point type           |
| `bool`   | boolean type                  |
| `string` | string type                   |
| `void`   | absent return type            |
| `true`   | boolean literal               |
| `false`  | boolean literal               |

---

## 2. Context-free grammar (EBNF)

The following grammar uses EBNF notation:

- `{ x }` – zero or more repetitions of *x*
- `[ x ]` – optional *x*
- `( a | b )` – choice between *a* and *b*

```ebnf
(* ── Top level ──────────────────────────────────────────────── *)

program     = func_decl { func_decl } ;

(* ── Declarations ───────────────────────────────────────────── *)

func_decl   = 'func' IDENT '(' param_list ')' '->' type block ;

param_list  = [ param { ',' param } ] ;

param       = IDENT ':' type ;

type        = 'int' | 'float' | 'bool' | 'string' | 'void' ;

(* ── Statements ─────────────────────────────────────────────── *)

block       = '{' { stmt } '}' ;

stmt        = var_decl
            | assign_stmt
            | if_stmt
            | while_stmt
            | for_stmt
            | return_stmt
            | print_stmt
            | expr_stmt ;

var_decl    = 'let' IDENT [ ':' type ] '=' expr ;

assign_stmt = IDENT '=' expr ;

if_stmt     = 'if' expr block [ 'else' block ] ;

while_stmt  = 'while' expr block ;

for_stmt    = 'for' IDENT 'in' 'range' '(' expr ',' expr ')' block ;

return_stmt = 'return' [ expr ] ;

print_stmt  = 'print' '(' expr ')' ;

expr_stmt   = expr ;

(* ── Expressions (ordered by ascending precedence) ──────────── *)

expr        = or_expr ;

or_expr     = and_expr  { 'or'  and_expr  } ;

and_expr    = eq_expr   { 'and' eq_expr   } ;

eq_expr     = cmp_expr  { ( '==' | '!=' ) cmp_expr  } ;

cmp_expr    = add_expr  { ( '<' | '>' | '<=' | '>=' ) add_expr } ;

add_expr    = mul_expr  { ( '+' | '-' ) mul_expr } ;

mul_expr    = unary     { ( '*' | '/' | '%' ) unary } ;

unary       = ( 'not' | '!' | '-' ) unary
            | primary ;

primary     = INTEGER
            | FLOAT
            | STRING
            | BOOL
            | IDENT [ '(' arg_list ')' ]
            | '(' expr ')' ;

arg_list    = [ expr { ',' expr } ] ;
```

---

## 3. Type system

ZLang is **statically typed**.  Every variable and expression has a known
type at compile time.

### Built-in types

| ZLang type | Description                         | C++ equivalent |
|------------|-------------------------------------|----------------|
| `int`      | 64-bit signed integer               | `int`          |
| `float`    | double-precision floating point     | `double`       |
| `bool`     | boolean (`true` / `false`)          | `bool`         |
| `string`   | immutable UTF-8 text                | `std::string`  |
| `void`     | absent value (only as return type)  | `void`         |

### Type inference

When the explicit type annotation is omitted in a `let` declaration, the
compiler infers the type from the right-hand-side expression:

```zlang
let x = 42       # inferred: int
let y = 3.14     # inferred: float
let ok = true    # inferred: bool
let s = "hi"     # inferred: string
let z = add(x,x) # inferred from return type of add()
```

### Implicit widening

An `int` value may be implicitly widened to `float` in assignments,
function calls, and return statements when the declared/expected type is
`float`.

### Operator type rules

| Operators         | Operand types          | Result type |
|-------------------|------------------------|-------------|
| `+ - * / %`       | int × int              | int         |
| `+ - * / %`       | float × float          | float       |
| `+ - * / %`       | int × float (any order)| float       |
| `+`               | string × string        | string      |
| `== !=`           | same type (or numeric) | bool        |
| `< > <= >=`       | numeric × numeric      | bool        |
| `and or`          | bool × bool            | bool        |
| `not`             | bool                   | bool        |
| unary `-`         | int or float           | same        |

---

## 4. Scoping rules

- Global scope contains only function names.
- Each function body introduces a new scope for its parameters.
- Each `block` (`{ … }`) introduces a new nested scope.
- `for` loop variables are scoped to the loop body.
- Variables are declared with `let`; re-declaration in the same scope is
  an error.
- Shadowing (declaring a variable with the same name in an inner scope) is
  **allowed**.

---

## 5. Compilation pipeline

```
ZLang source
     │
     ▼
┌─────────┐   tokens   ┌────────┐   AST   ┌──────────────────┐
│  Lexer  │──────────►│ Parser │────────►│ SemanticAnalyzer  │
└─────────┘            └────────┘         └──────────────────┘
                                                   │ annotated AST
                                                   ▼
                                         ┌──────────────────┐
                                         │  CodeGenerator   │
                                         └──────────────────┘
                                                   │ C++ source
                                                   ▼
                                              g++ / clang++
                                                   │ native binary
                                                   ▼
                                               execution
```
