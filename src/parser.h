// Задача парсера: преобразовать поток токенов (от лексера) в Abstract Syntax Tree (АСТ)
// 
// АСТ — это иерархическая структура, которая представляет программу в виде дерева:
//   - Корень дерева: Module (модуль = целый файл программы)
//   - Узлы дерева: объявления (namespace, type, struct, fn), statements (let, if, while, return),
//                  выражения (число, переменная, вызов функции, бинарная операция и т.д.)
//
// Архитектура парсера:
// 1. parseModule() — точка входа, читает весь файл
// 2. parseDeclaration() — читает верхнеуровневые объявления
// 3. parseStatement() — читает инструкции (let, var, if, while, return, блоки)
// 4. parseExpression() — читает выражения с правильными приоритетами операторов (Pratt parsing)
//
// Обработка ошибок:
// - Парсер НЕ бросает исключения при синтаксических ошибках
// - Ошибки накапливаются в diagnostics_ и выводятся в конце
// - Используется "паник-мод" для подавления каскадных ошибок и быстрого восстановления
// - После ошибки: panicMode_ = true, затем вызываем synchronize() для перехода к следующему элемента
//
// Позиции: для каждого элемента АСТ сохраняется его позиция в исходном коде
//          (нужна для точной диагностики ошибок)
//

#pragma once

#include "ast.h"
#include "lexer.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace Parser {
// Содержит: название файла, позицию (строка, колонка) и текст ошибки
// Примеры:
//   file="hello.astra", pos={5, 10}, message="ожидался ';' после выражения"
//   file="main.astra", pos={1, 1}, message="ожидалось верхнеуровневое объявление"
struct Diagnostic {
    std::string file;           // путь к исходному файлу
    Lexer::Position pos{};      // строка и колонка ошибки
    std::string message;        // текст диагностического сообщения
};

// Опции парсера: recoverErrors: если true, парсер пытается восстановиться после ошибок и продолжить парсинг
//                если false, парсер останавливается после первой ошибки
struct Options {
    bool recoverErrors = false;  // продолжать парсинг после ошибок?
};

// Входные данные: вектор токенов от лексера + имя файла
// Выходные данные: Abstract Syntax Tree (АСТ) — представление программы в виде дерева
//
// Основной метод: parseModule() → возвращает std::unique_ptr<AST::Module>
//
// Ошибки: накапливаются в diagnostics_, не выбрасываются как исключения
// Проверка ошибок: if (!parser.diagnostics().empty()) { обработать ошибки }
class Parser {
public:
    // Конструктор: принимает токены от лексера
    Parser(std::vector<Lexer::Token> tokens,
           std::string fileName,
           Options options = {});

    // ГЛАВНЫЙ МЕТОД: парсить весь модуль (файл) и вернуть АСТ
    // Вызывать этот метод один раз после создания Parser
    std::unique_ptr<AST::Module> parseModule();

    // Получить вектор всех ошибок, обнаруженных во время парсинга
    // Пустой вектор = парсинг успешен, нет ошибок
    const std::vector<Diagnostic>& diagnostics() const noexcept {
        return diagnostics_;
    }

private:
    // Внутреннее исключение для обработки синтаксических ошибок
    // НЕ выбрасывается! Используется для контроля потока выполнения
    struct ParseError : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    // состояние парсера
    std::vector<Lexer::Token> tokens_;      // входной поток токенов от лексера
    std::string fileName_;                  // имя исходного файла (для диагностики)
    Options options_{};                     // опции парсера
    std::vector<Diagnostic> diagnostics_;   // накопленные ошибки
    std::size_t current_ = 0;               // индекс текущего токена

    // Режим паники: подавляем каскадные ошибки
    // Когда встречаем ошибку → panicMode_ = true → пропускаем токены до точки синхронизации
    bool panicMode_ = false;

    // метода доступа к токенам
    // Все эти методы НЕ изменяют current_ (это методы просмотра)
    
    // Получить текущий токен без продвижения (смотрим, не берём)
    // lookahead=0 → текущий токен, lookahead=1 → следующий, lookahead=2 → через один, и т.д.
    const Lexer::Token& peek(std::size_t lookahead = 0) const;
    
    // Получить предыдущий токен (тот, который мы взяли последний раз через advance())
    const Lexer::Token& previous() const;
    
    // Проверка: дошли ли до конца файла (встретили ли EndOfFile)?
    bool isAtEnd() const;
    
    // Взять текущий токен и продвинуться к следующему (current_++)
    const Lexer::Token& advance();

    // проверка и согласование токенов
    // Проверить тип (и опционально текст) текущего токена БЕЗ продвижения
    // Возвращает true, если совпадает, false — нет
    // Примеры:
    //   check(TokenType::Keyword, "let") — это ключевое слово let?
    //   check(TokenType::Separator, ";") — это точка с запятой?
    bool check(Lexer::TokenType type, std::string_view lexeme = {}) const;
    
    // Проверить (как check), и если совпадает — также продвинуться
    // Возвращает true если совпадал и мы продвинулись, false если не совпадал
    bool match(Lexer::TokenType type, std::string_view lexeme = {});
    
    // Ожидать токен определённого типа и текста
    // Если совпадает: продвигаемся и возвращаем токен
    // Если НЕ совпадает: добавляем ошибку в diagnostics_, включаем panicMode_ и возвращаем текущий токен
    // НЕ бросает исключение!
    const Lexer::Token& expect(Lexer::TokenType type,
                               std::string_view lexeme,
                               std::string_view message);

    // ОБРАБОТКА ОШИБОК И ВОССТАНОВЛЕНИЕ ПОСЛЕ ОШИБОК
    // Добавить ошибку в diagnostics_
    // Если panicMode_ == true → подавляем ошибку (не добавляем, чтобы избежать каскада)
    void errorAt(const Lexer::Token& tok, std::string_view message);

    // Восстановиться после ошибки на верхнем уровне
    // Пропускаем токены до следующего верхнеуровневого ключевого слова:
    // module, namespace, type, struct, fn
    void synchronizeTopLevel();
    
    // Восстановиться после ошибки внутри блока/функции
    // Пропускаем токены до точки синхронизации:
    // конец инструкции (;), начало новой инструкции (let, var, if, while, break, continue, return, }
    void synchronizeStatement();
    
    // Может ли токен использоваться как имя переменной/функции?
    // Включает: Identifier и встроенные имена (print, input, len, exit, panic)
    bool isNameLike(const Lexer::Token& tok) const;
    
    // Может ли токен использоваться как имя типа?
    // Включает: Identifier и встроенный тип unit
    bool isTypeNameLike(const Lexer::Token& tok) const;
    
    // Ожидать идентификатор или встроенное имя, или добавить ошибку
    std::string expectIdentifierLike(std::string_view message);

    // Парсить путь имени: name или module::name или a::b::c
    std::vector<std::string> parseNamePath();
    
    // Парсить путь имени модуля (как parseNamePath, но для module объявлений)
    std::vector<std::string> parseModuleNamePath();

    // Создать SourceSpan (область кода) от первого до последнего токена
    // Используется для диагностики ошибок (на какой строке и колонке ошибка?)
    AST::SourceSpan spanFrom(const Lexer::Token& first,
                             const Lexer::Token& last) const;

    // Преобразовать строковый литерал из лексера в сторку
    // Обрабатывает escape-последовательности: \n, \t, \", \\
    // Входной параметр: лексема включает кавычки, например: "hello\nworld"
    // Выход: "hello\nworld" (с реальными символами переновода строки)
    static std::string decodeStringLiteral(std::string_view lexeme);
    
    // Парсить одно объявление верхнего уровня
    // Возможные типы: namespace, type (type alias), struct, fn (функция)
    AST::DeclPtr parseDeclaration();
    
    // Парсить namespace SomeName { ...decls... }
    std::unique_ptr<AST::NamespaceDecl> parseNamespaceDecl();
    
    // Парсить type alias: type MyInt = i32;
    std::unique_ptr<AST::TypeAliasDecl> parseTypeAliasDecl();
    
    // Парсить структуру: struct Point { x: i32; y: i32; }
    std::unique_ptr<AST::StructDecl> parseStructDecl();
    
    // Парсить функцию: fn add(a: i32, b: i32) -> i32 { ... }
    std::unique_ptr<AST::FunctionDecl> parseFunctionDecl();

    
    // Парсить выражение типа: i32, f64, [i32], module::Type, и т.д.
    AST::TypePtr parseTypeExpr();
    
    // Парсить одну инструкцию
    // Возможные типы: let, var, if, while, return, блок, присваивание, или просто выражение
    AST::StmtPtr parseStatement();
    
    // Парсить блок кода: { stmt1; stmt2; stmt3; }
    // Используется в: if/else, while, функции, и везде, где нужен блок инструкций
    std::unique_ptr<AST::BlockStmt> parseBlock();
    
    // Парсить let (неизменяемая переменная): let x = 42; или let x: i32 = 42;
    // После инициализации значение НЕЛЬЗЯ изменить
    AST::StmtPtr parseLetStmt();
    
    // Парсить var (изменяемая переменная): var x: i32 = 42;
    // После инициализации можно изменять значение: x = 100;
    // Тип ОБЯЗАТЕЛЕН для var (в отличие от let)
    AST::StmtPtr parseVarStmt();
    
    // Парсить условную инструкцию: if (условие) { ... } else { ... }
    AST::StmtPtr parseIfStmt();
    
    // Парсить цикл: while (условие) { ... }
    AST::StmtPtr parseWhileStmt();
    
    // Парсить возврат из функции: return; или return expr;
    AST::StmtPtr parseReturnStmt();

    // Выражение — это значение: число, переменная, операция, вызов функции и т.д.
    //
    // Иерархия приоритетов операторов (от низкого к высокому):
    //   1. || (логическое ИЛИ)
    //   2. && (логическое И)
    //   3. ==, != (сравнение на равенство)
    //   4. <, <=, >, >= (сравнение порядка)
    //   5. +, - (сложение, вычитание)
    //   6. *, /, % (умножение, деление, остаток)
    //   7. as (приведение типа)
    //   8. унарные операции и постфиксные операции (самый высокий приоритет)
    //
    // Методы парсинга:
    // parseExpression() → использует Pratt parsing для правильного учёта приоритетов
    // parseUnary() → унарные операции: -x, !x
    // parsePostfix() → постфиксные операции: x(), x.field, x[index], x::name
    // parsePrimary() → базовые выражения: литералы, переменные, скобки, структуры
    
    // Парсить выражение с минимальным приоритетом (Pratt parsing)
    // minPrec = минимальный приоритет, который мы ожидаем
    // Примеры:
    //   parseExpression(1) — парсить от ||(приоритет 1) и выше
    //   parseExpression(5) — парсить от +(приоритет 5) и выше
    AST::ExprPtr parseExpression(int minPrec = 1);
    
    // Парсить унарные операции: -число или !условие
    AST::ExprPtr parseUnary();
    
    // Парсить постфиксные операции: x(), x.field, x[index], x::name
    // Например: obj.field.subfield[0](arg1, arg2)
    AST::ExprPtr parsePostfix();
    
    // Парсить базовое выражение (первичное значение)
    // Может быть:
    //   - литерал: 42, 3.14, "hello", true
    //   - переменная/имя: x, module::Class::Member
    //   - вызов функции: func(arg1, arg2)
    //   - литерал структуры: Point { x: 1, y: 2 }
    //   - литерал массива: [1, 2, 3]
    //   - выражение в скобках: (x + y)
    AST::ExprPtr parsePrimary();
    
    // Проверить, может ли выражение использоваться как левая часть присваивания
    // Допустимые lvalue: переменная (x), поле (obj.field), элемент массива (arr[0])
    // НЕ допустимые lvalue: литерал (42), сложное выражение (x + y)
    bool isAssignable(const AST::Expr& expr) const;
    
    // Получить приоритет оператора (нужен для Pratt parsing)
    // Возвращает число от 0 (самый низкий) до 7 (самый высокий)
    // Примеры:
    //   || → 1, && → 2, == → 3, + → 5, * → 6, as → 7
    int precedenceOf(const Lexer::Token& tok) const;
    
    // Проверить, левоассоциативен ли оператор
    // Левоассоциативный: 10 - 5 - 2 = (10 - 5) - 2 = 3
    // Правоассоциативный: 2 ^ 3 ^ 2 = 2 ^ (3 ^ 2) = 2 ^ 9 = 512
    // В нашем языке все операторы левоассоциативные
    bool isLeftAssociative(const Lexer::Token& tok) const;
};

// Преобразовать диагностику в строку для вывода
// Формат: "filename:line:column: error: message"
// Пример: "hello.astra:5:10: error: ожидался ';' после выражения"
std::string formatDiagnostic(const Diagnostic& diagnostic);

} // namespace Parser