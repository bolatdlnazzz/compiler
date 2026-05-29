// Входные данные: std::vector<Lexer::Token> — поток токенов от лексера
// Выходные данные: std::unique_ptr<AST::Module> — Abstract Syntax Tree
//
// Алгоритм парсинга:
// 1. Конструктор инициализирует список токенов
// 2. Вызывается parseModule() — главный метод
// 3. parseModule() вызывает parseDeclaration() для каждого верхнеуровневого объявления
// 4. parseDeclaration() маршрутизирует парсинг по типам (fn, struct, namespace, type)
// 5. Каждый парсер (parseStatement, parseExpression и т.д.) рекурсивно вызывает друг друга
//
// Обработка ошибок (паник-мод):
// - При встречі синтаксической ошибки НЕ бросаем исключение
// - Вместо этого: добавляем в diagnostics_, устанавливаем panicMode_ = true
// - В паник-моде подавляем новые ошибки, чтобы избежать каскада сообщений
// - Вызываем synchronize() для перемотки до следующей точки восстановления
// - После синхронизации выключаем паник-мод
// - Обработка ошибок продолжается: можно собрать несколько ошибок за один проход
//
// Позиции в коде:
// - Каждый узел АСТ содержит SourceSpan (файл, начало, конец)
// - Используется для диагностики: "ошибка на строке 5, колонке 10"
// - Позиция берётся из токена: каждый токен помнит свою позицию в исходном коде
//

#include "parser.h"

#include <charconv>
#include <sstream>
#include <utility>

namespace Parser {

// конструктор парсера: Инициализирует парсер с полученными токенами от лексера
// Гарантирует, что список токенов заканчивается EndOfFile
Parser::Parser(std::vector<Lexer::Token> tokens, std::string fileName, Options options)
    : tokens_(std::move(tokens)), fileName_(std::move(fileName)), options_(options) {
    // Проверка: если токены пусты или не заканчиваются EndOfFile, добавляем его
    if (tokens_.empty() || tokens_.back().type != Lexer::TokenType::EndOfFile) {
        tokens_.push_back(Lexer::Token{Lexer::TokenType::EndOfFile, "", Lexer::Position{1, 1}});
    }
}

// Все методы доступа используют current_ как индекс в массиве tokens_
// current_ всегда указывает на "текущий" токен (который мы сейчас читаем)

// Получить токен с заданным смещением без продвижения
// Если индекс вышел за границы — возвращаем EndOfFile (токен в конце списка)
const Lexer::Token& Parser::peek(std::size_t lookahead) const {
    const std::size_t index = current_ + lookahead;
    if (index >= tokens_.size()) return tokens_.back();  // EndOfFile
    return tokens_[index];
}

// Получить предыдущий токен (тот, что мы взяли последний раз через advance())
const Lexer::Token& Parser::previous() const {
    if (current_ == 0) return tokens_.front();
    return tokens_[current_ - 1];
}

// Проверка: достигли ли конца файла?
bool Parser::isAtEnd() const {
    return peek().type == Lexer::TokenType::EndOfFile;
}

// Взять текущий токен и продвинуться к следующему
const Lexer::Token& Parser::advance() {
    if (!isAtEnd()) ++current_;  // переводим current_ на следующий токен
    return previous();           // возвращаем токен, который только что "съели"
}

// Проверить, совпадает ли текущий токен с типом (и опционально с текстом)
// Примеры:
//   check(TokenType::Keyword, "let") — это ключевое слово "let"?
//   check(TokenType::Separator, ";") — это точка с запятой?
bool Parser::check(Lexer::TokenType type, std::string_view lexeme) const {
    if (peek().type != type) return false;
    // Если lexeme не указана (пусто), проверяем только тип
    return lexeme.empty() || peek().lexeme == lexeme;
}

// Проверить (как check), и если совпадает — продвинуться
// Возвращает true если совпадал и мы продвинулись, false если не совпадал
bool Parser::match(Lexer::TokenType type, std::string_view lexeme) {
    if (!check(type, lexeme)) return false;
    advance();
    return true;
}

// expect: ожидать определённый токен
// Если найден → продвигаемся и возвращаем токен (успех)
// Если НЕ найден → добавляем ошибку, включаем panicMode_, возвращаем текущий токен (ошибка)
// НЕ бросает исключение!
const Lexer::Token& Parser::expect(Lexer::TokenType type,
                                   std::string_view lexeme,
                                   std::string_view message) {
    if (check(type, lexeme)) return advance();  // нашли, отлично
    errorAt(peek(), message);                   // не нашли, сообщаем об ошибке
    panicMode_ = true;                          // включаем паник-мод
    return peek();                              // возвращаем текущий токен (не продвигаемся)
}

// Добавить диагностику об ошибке
// Если panicMode_ == true → подавляем ошибку (избегаем каскада сообщений)
void Parser::errorAt(const Lexer::Token& tok, std::string_view message) {
    // Подавляем каскадные ошибки в режиме паники
    if (panicMode_) return;  // уже в режиме паники, не добавляем ещё ошибку
    diagnostics_.push_back(Diagnostic{fileName_, tok.pos, std::string(message)});
}

// После ошибки парсер находится в паник-моде и нужно перейти к следующему элементу
// Эта функция пропускает токены до "точки синхронизации"

// Синхронизация на верхнем уровне (пропускаем токены до начала следующего объявления)
void Parser::synchronizeTopLevel() {
    panicMode_ = false;  // выходим из паник-мода
    while (!isAtEnd()) {
        // Ищем начало нового верхнеуровневого объявления
        if (check(Lexer::TokenType::Keyword, "module") ||
            check(Lexer::TokenType::Keyword, "namespace") ||
            check(Lexer::TokenType::Keyword, "type") ||
            check(Lexer::TokenType::Keyword, "struct") ||
            check(Lexer::TokenType::Keyword, "fn")) {
            return;  // нашли начало следующего объявления
        }
        advance();  // пропускаем текущий токен
    }
}

// Синхронизация на уровне инструкций (пропускаем токены до конца инструкции или начала новой)
void Parser::synchronizeStatement() {
    panicMode_ = false;  // выходим из паник-мода
    while (!isAtEnd()) {
        // Признак конца инструкции — точка с запятой
        if (previous().lexeme == ";") return;
        
        // Или начало новой инструкции
        if (check(Lexer::TokenType::Keyword, "let") ||
            check(Lexer::TokenType::Keyword, "var") ||
            check(Lexer::TokenType::Keyword, "if") ||
            check(Lexer::TokenType::Keyword, "while") ||
            check(Lexer::TokenType::Keyword, "break") ||
            check(Lexer::TokenType::Keyword, "continue") ||
            check(Lexer::TokenType::Keyword, "return") ||
            check(Lexer::TokenType::Separator, "}")) {  // закрытие блока
            return;
        }
        advance();  // пропускаем текущий токен
    }
}

// Проверить: может ли токен использоваться как имя переменной или функции?
// Включает: обычные идентификаторы И встроенные имена (print, input, len, exit, panic, assert)
bool Parser::isNameLike(const Lexer::Token& tok) const {
    if (tok.type == Lexer::TokenType::Identifier) return true;
    // Встроенные имена, которые допускаются как обычные идентификаторы
    if (tok.type != Lexer::TokenType::Keyword) return false;
    return tok.lexeme == "print" || tok.lexeme == "input" || tok.lexeme == "exit" ||
           tok.lexeme == "panic" || tok.lexeme == "assert" || tok.lexeme == "unit" || tok.lexeme == "len";
}

// Проверить: может ли токен использоваться как имя типа?
// Включает: идентификаторы И встроенный тип unit
bool Parser::isTypeNameLike(const Lexer::Token& tok) const {
    return tok.type == Lexer::TokenType::Identifier ||
           (tok.type == Lexer::TokenType::Keyword && tok.lexeme == "unit");
}

// Ожидать идентификатор или встроенное имя, или вернуть ошибку
std::string Parser::expectIdentifierLike(std::string_view message) {
    if (peek().type == Lexer::TokenType::Identifier) return advance().lexeme;
    // Встроенные имена тоже разрешены как идентификаторы в нужных позициях
    if (peek().type == Lexer::TokenType::Keyword &&
        (peek().lexeme == "print" || peek().lexeme == "input" ||
         peek().lexeme == "exit" || peek().lexeme == "panic" || peek().lexeme == "assert" || peek().lexeme == "len")) {
        return advance().lexeme;
    }
    errorAt(peek(), message);
    panicMode_ = true;
    return "<error>";  // возвращаем значение ошибки
}

// Путь имени: простое имя или модульный путь через ::
// Примеры: "x", "Math::PI", "module::submodule::function"

std::vector<std::string> Parser::parseNamePath() {
    std::vector<std::string> path;
    
    // Первый компонент пути должен быть похож на имя или имя типа
    if (!isNameLike(peek()) && !isTypeNameLike(peek())) {
        errorAt(peek(), "ожидалось имя");
        panicMode_ = true;
        return {"<error>"};
    }
    path.push_back(advance().lexeme);  // добавляем первый компонент
    
    // Остальные компоненты после :: (если есть)
    while (match(Lexer::TokenType::Operator, "::")) {
        if (!isNameLike(peek()) && !isTypeNameLike(peek())) {
            errorAt(peek(), "ожидалось имя после '::'");
            panicMode_ = true;
            break;
        }
        path.push_back(advance().lexeme);
    }
    return path;
}

// Парсить путь имени модуля (как parseNamePath, но используется для module объявлений)
std::vector<std::string> Parser::parseModuleNamePath() {
    std::vector<std::string> path;
    path.push_back(expectIdentifierLike("ожидалось имя модуля"));
    while (match(Lexer::TokenType::Operator, "::")) {
        path.push_back(expectIdentifierLike("ожидалось имя модуля после '::'"));
    }
    return path;
}

// Создать SourceSpan — область кода от первого до последнего токена
// Используется для привязки ошибок к конкретному месту в коде
AST::SourceSpan Parser::spanFrom(const Lexer::Token& first, const Lexer::Token& last) const {
    return AST::SourceSpan{fileName_, first.pos, last.pos};
}

// Преобразовать строку из лексера (с кавычками и escape-последовательностями)
// в обычную строку с реальными управляющими символами
std::string Parser::decodeStringLiteral(std::string_view lexeme) {
    std::string result;
    if (lexeme.size() < 2) return result;  // слишком коротко, чтобы быть строкой
    
    // Пропускаем открывающую кавычку и пока не достигнем закрывающей
    for (std::size_t i = 1; i + 1 < lexeme.size(); ++i) {
        char c = lexeme[i];
        if (c == '\\' && i + 1 < lexeme.size() - 1) {
            // Это escape-последовательность
            char escaped = lexeme[++i];
            switch (escaped) {
                case 'n': result.push_back('\n'); break;  // переновод строки
                case 't': result.push_back('\t'); break;  // табуляция
                case '"': result.push_back('"'); break;   // кавычка
                case '\\': result.push_back('\\'); break; // бэкслеш
                default: result.push_back(escaped); break;
            }
        } else {
            result.push_back(c);
        }
    }
    return result;
}


char Parser::decodeCharLiteral(std::string_view lexeme) {
    if (lexeme.size() < 3) return '\0';
    if (lexeme[1] != '\\') return lexeme[1];
    if (lexeme.size() < 4) return '\0';
    switch (lexeme[2]) {
        case 'n': return '\n';
        case 't': return '\t';
        case '\'': return '\'';
        case '\\': return '\\';
        case '0': return '\0';
        default: return lexeme[2];
    }
}

// Точка входа парсера. Вызывается один раз и парсит весь исходный файл.
//
// Структура модуля:
// 1. Опциональное объявление module (если есть, должно быть в начале)
// 2. Список верхнеуровневых объявлений: namespace, type alias, struct, fn
//
// Алгоритм:
// - Проверяем заголовок module (если есть)
// - Читаем все верхнеуровневые объявления в цикле
// - Если встречаем ошибку → panicMode_, затем синхронизируемся и продолжаем
// - Возвращаем Module с собранными объявлениями
std::unique_ptr<AST::Module> Parser::parseModule() {
    auto module = std::make_unique<AST::Module>();
    const auto first = peek();

    // Заголовок module необязателен (может быть опущен)
    // Если присутствует, он ДОЛЖЕН быть в самом начале файла
    if (match(Lexer::TokenType::Keyword, "module")) {
        module->namePath = parseModuleNamePath();
        expect(Lexer::TokenType::Separator, ";", "ожидался ';' после объявления module");
        panicMode_ = false;  // сбрасываем паник-мод после успешного разбора заголовка
    }

    // Главный цикл парсинга верхнеуровневых объявлений
    while (!isAtEnd()) {
        // Проверка: module может быть только в начале
        if (check(Lexer::TokenType::Keyword, "module")) {
            errorAt(peek(), "объявление module допустимо только в начале файла");
            break;
        }
        
        // Обработка ошибок: если в паник-моде, синхронизируемся
        if (panicMode_) {
            synchronizeTopLevel();
            continue;
        }
        
        // Парсим одно объявление
        AST::DeclPtr decl = parseDeclaration();
        if (decl) module->decls.push_back(std::move(decl));
        
        // Если возникла ошибка и не восстанавливаемся — стоп
        if (panicMode_) {
            if (!options_.recoverErrors) break;  // если нет восстановления, выходим
            synchronizeTopLevel();  // иначе синхронизируемся и продолжаем
        }
    }

    // Сохраняем позицию модуля в исходном коде
    module->span = module->decls.empty()
        ? spanFrom(first, peek())
        : spanFrom(first, previous());
    return module;
}

// Маршрутизирует парсинг по типам объявлений:
// namespace SomeName { ... }
// type TypeAlias = i32;
// struct Point { x: i32; y: i32; }
// fn myFunc(a: i32) -> i32 { ... }
AST::DeclPtr Parser::parseDeclaration() {
    // Определяем тип объявления по ключевому слову и вызываем нужный парсер
    if (match(Lexer::TokenType::Keyword, "namespace")) return parseNamespaceDecl();
    if (match(Lexer::TokenType::Keyword, "type"))      return parseTypeAliasDecl();
    if (match(Lexer::TokenType::Keyword, "struct"))    return parseStructDecl();
    if (match(Lexer::TokenType::Keyword, "fn"))        return parseFunctionDecl();

    // Если нет известного ключевого слова → ошибка
    errorAt(peek(), "ожидалось верхнеуровневое объявление: namespace, type, struct или fn");
    panicMode_ = true;
    return nullptr;
}


// Синтаксис: namespace SomeName { ...objявления... }
// 
// Пример:
//   namespace Math {
//       fn add(a: i32, b: i32) -> i32 { ... }
//       fn mul(a: i32, b: i32) -> i32 { ... }
//   }
//
// Использование: Math::add(1, 2), Math::mul(3, 4)
std::unique_ptr<AST::NamespaceDecl> Parser::parseNamespaceDecl() {
    const auto first = previous();  // ключевое слово "namespace"
    auto node = std::make_unique<AST::NamespaceDecl>();
    
    // Имя пространства имён
    node->name = expectIdentifierLike("ожидалось имя пространства имён");
    if (panicMode_) return node;
    
    // Открывающая скобка
    expect(Lexer::TokenType::Separator, "{", "ожидался '{' после имени namespace");
    if (panicMode_) return node;

    // Парсим объявления внутри namespace (могут быть вложенные namespace, type, struct, fn)
    while (!isAtEnd() && !check(Lexer::TokenType::Separator, "}")) {
        if (panicMode_) { synchronizeTopLevel(); continue; }
        AST::DeclPtr decl = parseDeclaration();
        if (decl) node->decls.push_back(std::move(decl));
        if (panicMode_ && !options_.recoverErrors) return node;
        if (panicMode_) synchronizeTopLevel();
    }

    // Закрывающая скобка
    expect(Lexer::TokenType::Separator, "}", "ожидался '}' после namespace");
    panicMode_ = false;
    node->span = spanFrom(first, previous());
    return node;
}

// Синтаксис: type AliasName = i32;
//
// Пример:
//   type MyInt = i32;
//   type StringArray = [MyInt];
//
// Использование: переменная: MyInt = 42; // эквивалентно i32
std::unique_ptr<AST::TypeAliasDecl> Parser::parseTypeAliasDecl() {
    const auto first = previous();  // ключевое слово "type"
    auto node = std::make_unique<AST::TypeAliasDecl>();
    
    // Имя синонима
    node->name = expectIdentifierLike("ожидалось имя type alias");
    if (panicMode_) return node;
    
    // Знак равенства
    expect(Lexer::TokenType::Operator, "=", "ожидался '=' в объявлении type alias");
    if (panicMode_) return node;
    
    // Тип, для которого создаём синоним
    node->aliasedType = parseTypeExpr();
    if (panicMode_) return node;
    
    // Точка с запятой в конце
    expect(Lexer::TokenType::Separator, ";", "ожидался ';' после type alias");
    panicMode_ = false;
    node->span = spanFrom(first, previous());
    return node;
}

// Синтаксис: struct StructName { field1: type1; field2: type2; ... }
//
// Пример:
//   struct Point {
//       x: i32;
//       y: i32;
//   }
//
// Использование: let p = Point { x: 10, y: 20 };
std::unique_ptr<AST::StructDecl> Parser::parseStructDecl() {
    const auto first = previous();  // ключевое слово "struct"
    auto node = std::make_unique<AST::StructDecl>();
    
    // Имя структуры
    node->name = expectIdentifierLike("ожидалось имя структуры");
    if (panicMode_) return node;
    
    // Открывающая скобка
    expect(Lexer::TokenType::Separator, "{", "ожидался '{' после имени структуры");
    if (panicMode_) return node;

    // Парсим поля структуры
    while (!isAtEnd() && !check(Lexer::TokenType::Separator, "}")) {
        if (panicMode_) { synchronizeStatement(); continue; }
        
        // Описание одного поля: name: type;
        AST::FieldDecl field;
        const auto fieldFirst = peek();
        
        field.name = expectIdentifierLike("ожидалось имя поля структуры");
        if (panicMode_) { synchronizeStatement(); continue; }
        
        expect(Lexer::TokenType::Separator, ":", "ожидался ':' после имени поля");
        if (panicMode_) { synchronizeStatement(); continue; }
        
        field.type = parseTypeExpr();
        if (panicMode_) { synchronizeStatement(); continue; }
        
        expect(Lexer::TokenType::Separator, ";", "ожидался ';' после поля структуры");
        panicMode_ = false;
        field.span = spanFrom(fieldFirst, previous());
        node->fields.push_back(std::move(field));
    }

    // Закрывающая скобка
    expect(Lexer::TokenType::Separator, "}", "ожидался '}' после структуры");
    panicMode_ = false;
    node->span = spanFrom(first, previous());
    return node;
}

// Синтаксис: fn functionName(param1: type1, param2: type2, ...) [-> returnType] { body }
//
// Примеры:
//   fn add(a: i32, b: i32) -> i32 { a + b; }
//   fn greet(name: [i32]) { print(name); }  // без явного типа возврата → unit
//   fn processData() { var x: i32 = 0; }
std::unique_ptr<AST::FunctionDecl> Parser::parseFunctionDecl() {
    const auto first = previous();  // ключевое слово "fn"
    auto node = std::make_unique<AST::FunctionDecl>();
    
    // Имя функции
    node->name = expectIdentifierLike("ожидалось имя функции");
    if (panicMode_) return node;

    // Открывающая скобка списка параметров
    expect(Lexer::TokenType::Separator, "(", "ожидался '(' после имени функции");
    if (panicMode_) return node;

    // Парсим параметры функции
    if (!check(Lexer::TokenType::Separator, ")")) {
        do {
            if (panicMode_) break;
            
            AST::Param param;
            const auto paramFirst = peek();
            
            // Имя параметра
            param.name = expectIdentifierLike("ожидалось имя параметра");
            if (panicMode_) break;
            
            // Тип параметра (обязателен)
            expect(Lexer::TokenType::Separator, ":", "ожидался ':' перед типом параметра");
            if (panicMode_) break;
            
            param.type = parseTypeExpr();
            if (panicMode_) break;
            
            param.span = spanFrom(paramFirst, previous());
            node->params.push_back(std::move(param));
        } while (!panicMode_ && match(Lexer::TokenType::Separator, ","));
    }

    // Закрывающая скобка списка параметров
    if (panicMode_) return node;
    expect(Lexer::TokenType::Separator, ")", "ожидался ')' после списка параметров");
    if (panicMode_) return node;

    // Опциональный тип возврата (если есть -> type)
    if (match(Lexer::TokenType::Separator, "->")) {
        node->returnType = parseTypeExpr();
        if (panicMode_) return node;
    }
    // Если тип возврата не указан, функция возвращает unit (ничего)

    // Тело функции — блок инструкций
    node->body = parseBlock();
    panicMode_ = false;
    node->span = spanFrom(first, previous());
    return node;
}

AST::TypePtr Parser::parseTypeExpr() {  // Парсить тип: имя, массив [тип; размер]
    const auto first = peek();

    if (match(Lexer::TokenType::Separator, "[")) {  // Это массив [type; size]?
        auto node = std::make_unique<AST::ArrayType>();  // Создаём узел ArrayType
        node->elementType = parseTypeExpr();  // Рекурсивно парсим тип элемента
        if (panicMode_) return node;  // Если ошибка — выходим
        expect(Lexer::TokenType::Separator, ";", "ожидался ';' между типом элемента и размером массива");  // Разделитель ';'
        if (panicMode_) return node;
        const auto& sizeTok = expect(Lexer::TokenType::IntLiteral, {}, "ожидался целочисленный размер массива");  // Ожидаем число
        if (panicMode_) return node;  // Ошибка парсинга числа
        std::uint64_t size = 0;  // Переменная для размера
        int base = 10;
        std::string_view digits = sizeTok.lexeme;
        if (digits.size() > 2 && digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) {
            base = 16;
            digits.remove_prefix(2);
        } else if (digits.size() > 2 && digits[0] == '0' && (digits[1] == 'b' || digits[1] == 'B')) {
            base = 2;
            digits.remove_prefix(2);
        }
        auto begin = digits.data();  // Начало строки числа
        auto end = digits.data() + digits.size();  // Конец строки числа
        auto [ptr, ec] = std::from_chars(begin, end, size, base);  // Преобразуем строку в uint64_t
        if (ec != std::errc{} || ptr != end) {  // Если ошибка преобразования
            errorAt(sizeTok, "некорректный размер массива");  // Сообщаем ошибку
        }
        node->size = size;  // Сохраняем размер
        expect(Lexer::TokenType::Separator, "]", "ожидался ']' после типа массива");  // Закрывающая скобка
        panicMode_ = false;  // Выходим из паник-мода
        node->span = spanFrom(first, previous());  // Сохраняем позицию в коде
        return node;  // Возвращаем узел ArrayType
    }

    if (!isTypeNameLike(peek())) {  // Это не имя типа? Ошибка
        errorAt(peek(), "ожидался тип");  // Сообщаем об ошибке
        panicMode_ = true;  // Включаем паник-мод
        auto err = std::make_unique<AST::NamedType>();  // Создаём ошибочный узел
        err->path = {"<error>"};  // Помечаем как ошибку
        return err;  // Возвращаем ошибочный узел
    }

    auto node = std::make_unique<AST::NamedType>();  // Создаём NamedType (обычный тип)
    node->path.push_back(advance().lexeme);  // Добавляем первый компонент имени
    while (match(Lexer::TokenType::Operator, "::")) {  // Есть ещё компоненты через ::
        if (!isTypeNameLike(peek())) {  // Проверяем следующий компонент
            errorAt(peek(), "ожидалось имя типа после '::'");  // Ошибка
            panicMode_ = true;  // Паник-мод
            break;  // Выходим из цикла
        }
        node->path.push_back(advance().lexeme);  // Добавляем компонент (например, module::Type)
    }
    node->span = spanFrom(first, previous());  // Позиция типа в коде
    return node;  // Возвращаем узел NamedType
}

AST::StmtPtr Parser::parseStatement() {  // Парсить одну инструкцию
    if (match(Lexer::TokenType::Separator, ";")) {  // Пустая инструкция ';'
        auto s = std::make_unique<AST::EmptyStmt>();  // Создаём EmptyStmt
        s->span = spanFrom(previous(), previous());  // Позиция
        return s;  // Возвращаем
    }
    if (match(Lexer::TokenType::Keyword, "let"))      return parseLetStmt();  // let имя = значение;
    if (match(Lexer::TokenType::Keyword, "var"))      return parseVarStmt();  // var имя: тип = значение;
    if (match(Lexer::TokenType::Keyword, "if"))       return parseIfStmt();   // if (условие) блок [else ...]
    if (match(Lexer::TokenType::Keyword, "while"))    return parseWhileStmt();  // while (условие) блок
    if (match(Lexer::TokenType::Keyword, "break")) {  // break; — выход из цикла
        const auto first = previous();  // Позиция break
        auto s = std::make_unique<AST::BreakStmt>();  // Создаём BreakStmt
        expect(Lexer::TokenType::Separator, ";", "ожидался ';' после break");  // Требуем ;
        panicMode_ = false;  // Выходим из паник-мода
        s->span = spanFrom(first, previous());  // Позиция инструкции
        return s;  // Возвращаем
    }
    if (match(Lexer::TokenType::Keyword, "continue")) {  // continue; — переход к следующей итерации
        const auto first = previous();  // Позиция continue
        auto s = std::make_unique<AST::ContinueStmt>();  // Создаём ContinueStmt
        expect(Lexer::TokenType::Separator, ";", "ожидался ';' после continue");  // Требуем ;
        panicMode_ = false;  // Выходим из паник-мода
        s->span = spanFrom(first, previous());  // Позиция инструкции
        return s;  // Возвращаем
    }
    if (match(Lexer::TokenType::Keyword, "return")) return parseReturnStmt();  // return [значение];
    if (check(Lexer::TokenType::Separator, "{"))    return parseBlock();  // { ... } — блок инструкций

    // Сначала читаем выражение, а потом решаем: это присваивание или просто statement.
    const auto first = peek();  // Запоминаем начало
    auto lhsOrExpr = parseExpression();  // Парсим выражение
    if (panicMode_) return std::make_unique<AST::EmptyStmt>();  // Ошибка

    if (match(Lexer::TokenType::Operator, "=")) {  // Это присваивание?
        if (!isAssignable(*lhsOrExpr)) {  // Может ли быть слева от =
            errorAt(previous(), "левая часть присваивания должна быть lvalue (имя переменной, поле, элемент массива)");  // Ошибка
        }
        auto value = parseExpression();  // Парсим правую часть
        if (panicMode_) return std::make_unique<AST::EmptyStmt>();  // Ошибка
        expect(Lexer::TokenType::Separator, ";", "ожидался ';' после присваивания");  // Требуем ;
        panicMode_ = false;  // Выходим из паник-мода

        auto s = std::make_unique<AST::AssignStmt>();  // Создаём AssignStmt
        s->target = std::move(lhsOrExpr);  // Цель присваивания
        s->value = std::move(value);  // Значение
        s->span = spanFrom(first, previous());  // Позиция
        return s;  // Возвращаем
    }

    expect(Lexer::TokenType::Separator, ";", "ожидался ';' после выражения");  // Просто выражение, требуем ;
    panicMode_ = false;  // Выходим из паник-мода
    auto s = std::make_unique<AST::ExprStmt>();  // Создаём ExprStmt
    s->expr = std::move(lhsOrExpr);  // Сохраняем выражение
    s->span = spanFrom(first, previous());  // Позиция
    return s;  // Возвращаем
}

std::unique_ptr<AST::BlockStmt> Parser::parseBlock() {  // Парсить блок: { stmt1; stmt2; ... }
    // Блок всегда начинается с '{' и заканчивается '}'
    const auto first = expect(Lexer::TokenType::Separator, "{", "ожидался '{' в начале блока");  // Начинаем блок
    if (panicMode_) {  // Ошибка при косности '{'?
        auto block = std::make_unique<AST::BlockStmt>();  // Создаём пустый блок
        block->span = spanFrom(first, first);  // Позиция
        return block;  // Возвращаем
    }
    auto block = std::make_unique<AST::BlockStmt>();  // Создаём блок

    while (!isAtEnd() && !check(Lexer::TokenType::Separator, "}")) {  // Пока не найдём '}' и не EOF
        if (panicMode_) {  // Находимся в паник-моде?
            synchronizeStatement();  // Синхронизируемся
            panicMode_ = false;  // Выходим из паник-мода
            continue;  // Продолжаем
        }
        AST::StmtPtr stmt = parseStatement();  // Парсим инструкцию
        if (stmt) block->statements.push_back(std::move(stmt));  // Добавляем в блок
        if (panicMode_) {  // Если ошибка
            if (!options_.recoverErrors) break;  // Не восстанавливаем — выходим
            synchronizeStatement();  // Восстанавливаем
            panicMode_ = false;  // Выключаем паник-мод
        }
    }

    expect(Lexer::TokenType::Separator, "}", "ожидался '}' в конце блока");  // Кончаем блок
    panicMode_ = false;  // Выходим из паник-мода
    block->span = spanFrom(first, previous());  // Позиция блока
    return block;  // Возвращаем блок
}

AST::StmtPtr Parser::parseLetStmt() {
    const auto first = previous();
    auto s = std::make_unique<AST::LetStmt>();
    s->name = expectIdentifierLike("ожидалось имя переменной после let");
    if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    if (match(Lexer::TokenType::Separator, ":")) {
        s->explicitType = parseTypeExpr();
        if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    }
    expect(Lexer::TokenType::Operator, "=", "ожидался '=' в объявлении let");
    if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    s->initializer = parseExpression();
    if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    expect(Lexer::TokenType::Separator, ";", "ожидался ';' после объявления let");
    panicMode_ = false;
    s->span = spanFrom(first, previous());
    return s;
}

AST::StmtPtr Parser::parseVarStmt() {
    const auto first = previous();
    auto s = std::make_unique<AST::VarStmt>();
    s->name = expectIdentifierLike("ожидалось имя переменной после var");
    if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    expect(Lexer::TokenType::Separator, ":", "var требует явного типа и ':' после имени");
    if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    s->explicitType = parseTypeExpr();
    if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    expect(Lexer::TokenType::Operator, "=", "ожидался '=' в объявлении var");
    if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    s->initializer = parseExpression();
    if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    expect(Lexer::TokenType::Separator, ";", "ожидался ';' после объявления var");
    panicMode_ = false;
    s->span = spanFrom(first, previous());
    return s;
}

AST::StmtPtr Parser::parseIfStmt() {
    const auto first = previous();
    auto s = std::make_unique<AST::IfStmt>();
    expect(Lexer::TokenType::Separator, "(", "ожидался '(' после if");
    if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    s->condition = parseExpression();
    if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    expect(Lexer::TokenType::Separator, ")", "ожидался ')' после условия if");
    if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    s->thenBlock = parseBlock();
    if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    if (match(Lexer::TokenType::Keyword, "else")) {
        if (check(Lexer::TokenType::Separator, "{")) {
            s->elseBranch = parseBlock();
        } else if (match(Lexer::TokenType::Keyword, "if")) {
            s->elseBranch = parseIfStmt();
        } else {
            errorAt(peek(), "ожидался блок или if после else");
            panicMode_ = true;
            return std::make_unique<AST::EmptyStmt>();
        }
    }
    panicMode_ = false;
    s->span = spanFrom(first, previous());
    return s;
}

AST::StmtPtr Parser::parseWhileStmt() {
    const auto first = previous();
    auto s = std::make_unique<AST::WhileStmt>();
    expect(Lexer::TokenType::Separator, "(", "ожидался '(' после while");
    if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    s->condition = parseExpression();
    if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    expect(Lexer::TokenType::Separator, ")", "ожидался ')' после условия while");
    if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    s->body = parseBlock();
    panicMode_ = false;
    s->span = spanFrom(first, previous());
    return s;
}

AST::StmtPtr Parser::parseReturnStmt() {
    // return может быть с выражением или без (для unit-функций)
    const auto first = previous();
    auto s = std::make_unique<AST::ReturnStmt>();
    if (!check(Lexer::TokenType::Separator, ";")) {
        s->value = parseExpression();
        if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    }
    expect(Lexer::TokenType::Separator, ";", "ожидался ';' после return");
    panicMode_ = false;
    s->span = spanFrom(first, previous());
    return s;
}

AST::ExprPtr Parser::parseExpression(int minPrec) {  // Pratt parsing — правильный ордер операторов
    // minPrec: минимальный приоритет, если стоит рядом градуся квазиости
    auto left = parseUnary();  // Надстройка: унарные операторы (-x, !x)
    if (panicMode_) return left;  // Ошибка

    while (true) {  // Цикл: читаем бинарные операторы
        const Lexer::Token& opTok = peek();  // От какого токена читаем?
        const int prec = precedenceOf(opTok);  // Каков приоритет?
        if (prec < minPrec) break;  // Если приоритет ниже или не оператор — стоп

        advance();  // Ныряем оператор
        if (opTok.type == Lexer::TokenType::Keyword && opTok.lexeme == "as") {  // Оператор преобразования типа
            auto target = parseTypeExpr();  // Новый тип
            if (panicMode_) return left;  // Ошибка
            auto cast = std::make_unique<AST::CastExpr>();  // Узел CastExpr
            cast->value = std::move(left);  // На что кастируем
            cast->targetType = std::move(target);  // Новый тип
            cast->span = spanFrom(opTok, previous());  // Позиция
            left = std::move(cast);  // Мовим в left
            continue;  // Продолжаем цикл
        }

        const int nextMinPrec = isLeftAssociative(opTok) ? prec + 1 : prec;  // Оператор лево-асос => тяють
        auto right = parseExpression(nextMinPrec);  // Парсим правую часть
        if (panicMode_) return left;  // Ошибка

        auto binary = std::make_unique<AST::BinaryExpr>();  // Узел бинарного выражения
        binary->op = opTok.lexeme;  // Оператор
        binary->left = std::move(left);  // Левая часть
        binary->right = std::move(right);  // Правая часть
        binary->span = spanFrom(opTok, previous());  // Позиция
        left = std::move(binary);  // Обновляем left
    }

    return left;  // Возвращаем стойкость
}

AST::ExprPtr Parser::parseUnary() {  // Однооперандные: -x или !x
    if (check(Lexer::TokenType::Operator, "-") || check(Lexer::TokenType::Operator, "!")) {  // Унарные операторы
        const auto opTok = advance();  // Ныряем оператор
        auto node = std::make_unique<AST::UnaryExpr>();  // Узел unary
        node->op = opTok.lexeme;  // Оператор
        node->operand = parseUnary();  // Провим рекурсия: для !!x, ---x
        node->span = spanFrom(opTok, previous());  // Позиция
        return node;  // Возвращаем
    }
    return parsePostfix();  // Не unary — пытаем postfix
}

AST::ExprPtr Parser::parsePostfix() {
    auto expr = parsePrimary();
    if (panicMode_) return expr;

    while (true) {
        if (panicMode_) break;
        if (match(Lexer::TokenType::Separator, "(")) {
            const auto first = previous();
            auto call = std::make_unique<AST::CallExpr>();
            call->callee = std::move(expr);
            if (!check(Lexer::TokenType::Separator, ")")) {
                do {
                    if (panicMode_) break;
                    call->args.push_back(parseExpression());
                } while (!panicMode_ && match(Lexer::TokenType::Separator, ","));
            }
            if (panicMode_) return call;
            expect(Lexer::TokenType::Separator, ")", "ожидался ')' после аргументов вызова");
            panicMode_ = false;
            call->span = spanFrom(first, previous());
            expr = std::move(call);
            continue;
        }

        if (match(Lexer::TokenType::Operator, ".")) {
            const auto first = previous();
            auto field = std::make_unique<AST::FieldExpr>();
            field->object = std::move(expr);
            field->field = expectIdentifierLike("ожидалось имя поля после '.'");
            if (panicMode_) return field;
            field->span = spanFrom(first, previous());
            expr = std::move(field);
            continue;
        }

        if (match(Lexer::TokenType::Separator, "[")) {
            const auto first = previous();
            auto index = std::make_unique<AST::IndexExpr>();
            index->object = std::move(expr);
            index->index = parseExpression();
            if (panicMode_) return index;
            expect(Lexer::TokenType::Separator, "]", "ожидался ']' после индекса");
            panicMode_ = false;
            index->span = spanFrom(first, previous());
            expr = std::move(index);
            continue;
        }

        if (match(Lexer::TokenType::Operator, "::")) {
            if (auto* name = dynamic_cast<AST::NameExpr*>(expr.get())) {
                std::string next = expectIdentifierLike("ожидалось имя после '::'");
                if (panicMode_) return expr;
                name->path.push_back(std::move(next));
                continue;
            }
            errorAt(previous(), "оператор '::' допустим только для имени");
            panicMode_ = true;
            break;
        }

        break;
    }

    return expr;
}

AST::ExprPtr Parser::parsePrimary() {
    const auto first = peek();

    if (match(Lexer::TokenType::IntLiteral)) {
        auto e = std::make_unique<AST::IntLiteralExpr>();
        e->lexeme = previous().lexeme;
        e->span = spanFrom(first, previous());
        return e;
    }
    if (match(Lexer::TokenType::FloatLiteral)) {
        auto e = std::make_unique<AST::FloatLiteralExpr>();
        e->lexeme = previous().lexeme;
        e->span = spanFrom(first, previous());
        return e;
    }
    if (match(Lexer::TokenType::BoolLiteral)) {
        auto e = std::make_unique<AST::BoolLiteralExpr>();
        e->value = previous().lexeme == "true";
        e->span = spanFrom(first, previous());
        return e;
    }
    if (match(Lexer::TokenType::CharLiteral)) {
        auto e = std::make_unique<AST::CharLiteralExpr>();
        e->value = decodeCharLiteral(previous().lexeme);
        e->span = spanFrom(first, previous());
        return e;
    }
    if (match(Lexer::TokenType::StringLiteral)) {
        auto e = std::make_unique<AST::StringLiteralExpr>();
        e->value = decodeStringLiteral(previous().lexeme);
        e->span = spanFrom(first, previous());
        return e;
    }

    if (match(Lexer::TokenType::Separator, "(")) {
        auto expr = parseExpression();
        if (panicMode_) return expr;
        expect(Lexer::TokenType::Separator, ")", "ожидался ')' после выражения");
        panicMode_ = false;
        return expr;
    }

    if (match(Lexer::TokenType::Separator, "[")) {
        auto e = std::make_unique<AST::ArrayLiteralExpr>();
        if (!check(Lexer::TokenType::Separator, "]")) {
            do {
                if (panicMode_) break;
                e->elements.push_back(parseExpression());
            } while (!panicMode_ && match(Lexer::TokenType::Separator, ","));
        }
        if (panicMode_) return e;
        expect(Lexer::TokenType::Separator, "]", "ожидался ']' после литерала массива");
        panicMode_ = false;
        e->span = spanFrom(first, previous());
        return e;
    }

    if (isNameLike(peek()) || isTypeNameLike(peek())) {
        auto path = parseNamePath();
        if (panicMode_) {
            auto e = std::make_unique<AST::NameExpr>();
            e->path = std::move(path);
            return e;
        }
        // Проверяем: это литерал структуры вида Type { field: val, ... }?
        if (match(Lexer::TokenType::Separator, "{")) {
            auto e = std::make_unique<AST::StructLiteralExpr>();
            e->typePath = std::move(path);
            if (!check(Lexer::TokenType::Separator, "}")) {
                do {
                    if (panicMode_) break;
                    AST::StructFieldInit field;
                    const auto fieldFirst = peek();
                    field.name = expectIdentifierLike("ожидалось имя поля в литерале структуры");
                    if (panicMode_) break;
                    expect(Lexer::TokenType::Separator, ":", "ожидался ':' после имени поля");
                    if (panicMode_) break;
                    field.value = parseExpression();
                    if (panicMode_) break;
                    field.span = spanFrom(fieldFirst, previous());
                    e->fields.push_back(std::move(field));
                } while (!panicMode_ && match(Lexer::TokenType::Separator, ","));
            }
            if (panicMode_) return e;
            expect(Lexer::TokenType::Separator, "}", "ожидался '}' после литерала структуры");
            panicMode_ = false;
            e->span = spanFrom(first, previous());
            return e;
        }

        auto e = std::make_unique<AST::NameExpr>();
        e->path = std::move(path);
        e->span = spanFrom(first, previous());
        return e;
    }

    errorAt(peek(), "ожидалось выражение");
    panicMode_ = true;
    auto errorExpr = std::make_unique<AST::NameExpr>();
    errorExpr->path = {"<error>"};
    errorExpr->span = spanFrom(first, peek());
    return errorExpr;
}

bool Parser::isAssignable(const AST::Expr& expr) const {  // Можно ли не слева от =?
    return dynamic_cast<const AST::NameExpr*>(&expr) != nullptr ||  // Никнейм? Lvalue
           dynamic_cast<const AST::FieldExpr*>(&expr) != nullptr ||  // Поле? Lvalue
           dynamic_cast<const AST::IndexExpr*>(&expr) != nullptr;    // Элемент массива? Lvalue
}

int Parser::precedenceOf(const Lexer::Token& tok) const {  // Каков приоритет оператора?
    if (tok.type == Lexer::TokenType::Keyword && tok.lexeme == "as") return 7;  // as: высочайший приоритет
    if (tok.type != Lexer::TokenType::Operator) return 0;  // Не оператор? Выходим
    if (tok.lexeme == "||") return 1;  // Логическое диажункция: проритет 1
    if (tok.lexeme == "&&") return 2;  // Логическая конъюнкция: приоритет 2
    if (tok.lexeme == "==" || tok.lexeme == "!=") return 3;  // Равенство: приоритет 3
    if (tok.lexeme == "<" || tok.lexeme == "<=" || tok.lexeme == ">" || tok.lexeme == ">=") return 4;  // Сравнение: приоритет 4
    if (tok.lexeme == "+" || tok.lexeme == "-") return 5;  // Сложение/вычитание: приоритет 5
    if (tok.lexeme == "*" || tok.lexeme == "/" || tok.lexeme == "%") return 6;  // Умножение: приоритет 6
    return 0;  // Ункновный оператор = не нужен
}

bool Parser::isLeftAssociative(const Lexer::Token&) const {  // Все операторы лево-ассоциативные
    return true;  // a + b + c = (a + b) + c
}

std::string formatDiagnostic(const Diagnostic& diagnostic) {
    return diagnostic.file + ":" + std::to_string(diagnostic.pos.line) + ":" +
           std::to_string(diagnostic.pos.column) + ": error: " + diagnostic.message;
}

} // namespace Parser
