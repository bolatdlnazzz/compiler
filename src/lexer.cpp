#include "lexer.h"

#include <cctype>
#include <string>
#include <unordered_set>
#include <utility>

// ════════════════════════════════════════════════════════════════════════════════
// РЕАЛИЗАЦИЯ ЛЕКСИЧЕСКОГО АНАЛИЗАТОРА
// ════════════════════════════════════════════════════════════════════════════════
// Основная задача: разбить исходный текст программы на токены (смысловые единицы).
// Процесс:
//   1. Читаем символ за символом
//   2. Группируем символы в осмысленные единицы: слова, числа, строки, операторы
//   3. Каждый токен содержит: тип (Keyword, Identifier, Number и т.д.), текст и позицию
//   4. Парсер получает список токенов и строит из них синтаксическое дерево (AST)

namespace Lexer {

// Конструктор: инициализируем исходный код и имя файла
// source_  — весь текст программы целиком
// file_    — имя файла (для красивых сообщений об ошибках: "file.astra:5:10: error: ...")
// Позиция инициализируется с (index_=0, line_=1, column_=1)
Lexer::Lexer(std::string_view src, std::string filename)
    : source_(src), file_(std::move(filename)) {}

// Сбросить состояние лексера на начало: полезно для переобработки того же файла
void Lexer::reset() {
    index_ = 0;   // вернулись в начало текста
    line_ = 1;    // первая строка
    column_ = 1;  // первая колонка
}

const std::string& Lexer::filename() const {
    return file_;
}

// Посмотреть текущий символ БЕЗ продвижения указателя (peek = подглядеть)
// Возвращаем '\0' (null-терминатор) если конец файла — это сигнал парсеру "больше нечего"
char Lexer::peek() const {
    if (isAtEnd()) return '\0';
    return source_[index_];
}

// Посмотреть СЛЕДУЮЩИЙ символ (через один) БЕЗ продвижения
// Нужно для распознавания двухсимвольных операторов типа ==, ->, ::
char Lexer::peekNext() const {
    if (index_ + 1 >= source_.size()) return '\0';
    return source_[index_ + 1];
}

// Прочитать текущий символ И продвинуться на один символ вперёд
// КРИТИЧНО: одновременно обновляем line_ и column_ для отслеживания позиции
// Это нужно для красивых сообщений об ошибках: "line 5, column 10"
char Lexer::advance() {
    if (isAtEnd()) return '\0';

    char c = source_[index_++];  // читаем текущий, затем сдвигаемся
    if (c == '\n') {
        // Встретили перевод строки: счётчик строк ↑, колонка ↓ на 1
        ++line_;
        column_ = 1;
    } else {
        // Обычный символ: просто увеличиваем колонку
        ++column_;
    }
    return c;
}

// Проверить, совпадает ли текущий символ с ожиданием
// Если совпадает → продвигаемся вперёд (advance) и возвращаем true
// Если НЕ совпадает → остаемся на месте, возвращаем false
// Используется для распознавания двухсимвольных операторов:
//   Пример: если видим '=', проверяем match('=') → если true, это "=="
bool Lexer::match(char expected) {
    if (isAtEnd()) return false;
    if (source_[index_] != expected) return false;
    advance();  // только если совпадает!
    return true;
}

// Проверить: дошли ли до конца файла?
bool Lexer::isAtEnd() const {
    return index_ >= source_.size();
}

// Создать успешный токен: тип + текст (lexeme) + позиция
Token Lexer::makeToken(TokenType type, const std::string& lexeme, Position startPos) const {
    return Token{type, lexeme, startPos};
}

// Создать ОШИБОЧНЫЙ токен: сообщение об ошибке идёт в поле lexeme, тип = Error
Token Lexer::errorToken(const std::string& message, Position startPos) const {
    return Token{TokenType::Error, message, startPos};
}

// Проверить: буква ли это? (a-z, A-Z, _)
// Кастим в unsigned char для безопасности std::isalpha()
bool Lexer::isAlpha(char c) {
    unsigned char uc = static_cast<unsigned char>(c);
    return std::isalpha(uc) != 0;
}

// Проверить: цифра ли это? (0-9)
bool Lexer::isDigit(char c) {
    unsigned char uc = static_cast<unsigned char>(c);
    return std::isdigit(uc) != 0;
}

// Проверить: буква или цифра? (используется в идентификаторах типа myVar123)
bool Lexer::isAlphaNumeric(char c) {
    return isAlpha(c) || isDigit(c);
}

// Пропустить все пробелы, табуляции и переводы строк
// Эти символы не являются токенами, парсер их не видит
void Lexer::skipWhitespace() {
    while (!isAtEnd()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();  // просто пропускаем
        } else {
            break;  // встретили не-пробел, выходим
        }
    }
}

// Пропустить однострочный комментарий (всё от // до конца строки)
// Предполагается, что // уже распознаны, а мы пропускаем всё остальное
void Lexer::skipComment() {
    while (!isAtEnd() && peek() != '\n') {
        advance();  // читаем символ за символом до конца строки
    }
}

// Пропустить всё, что не несёт смысла для парсера: пробелы И комментарии
// Это главный метод очистки перед читаемым токеном
// Цикл: пробелы → проверка /? → пробелы → проверка и т.д.
void Lexer::skipWhitespaceAndComments() {
    while (!isAtEnd()) {
        skipWhitespace();  // пропускаем пробелы
        if (peek() == '/' && peekNext() == '/') {
            skipComment();  // нашли //, пропускаем комментарий
            continue;       // может быть ещё пробелы после комментария
        }
        break;  // нет комментария, выходим
    }
}

// ════════════════════════════════════════════════════════════════════════════════
// nextToken() — главный метод лексера
// ════════════════════════════════════════════════════════════════════════════════
// Получить один СЛЕДУЮЩИЙ ЗНАЧИМЫЙ токен из исходного кода
// 
// Алгоритм:
// 1. Пропустить пробелы и комментарии (они не токены)
// 2. Запомнить текущую позицию (нужна для ошибок и отладки)
// 3. Прочитать первый символ токена
// 4. По первому символу определить тип и вызвать нужный распознаватель:
//    - Буква/подчёркивание → идентификатор или ключевое слово
//    - Цифра → число (целое или дробное)
//    - Кавычка → строка
//    - Специальный символ → оператор или разделитель
// 5. Вернуть готовый токен
Token Lexer::nextToken() {
    skipWhitespaceAndComments();

    Position startPos{line_, column_};  // сохраняем позицию для диагностики ошибок
    if (isAtEnd()) {
        return makeToken(TokenType::EndOfFile, "", startPos);
    }

    char c = advance();  // читаем первый символ токена

    // ────────────────────────────────────────────────────────────────────────────
    // Определяем ТИП токена по первому символу
    // ────────────────────────────────────────────────────────────────────────────
    if (isAlpha(c) || c == '_') {
        return identifierOrKeyword(startPos);  // переменная/функция или ключевое слово
    }
    if (isDigit(c)) {
        return number(startPos);  // число
    }
    if (c == '"') {
        return stringLiteral(startPos);  // строка
    }

    // ────────────────────────────────────────────────────────────────────────────
    // Двухсимвольные операторы и разделители
    // Нужно проверить: это один символ или два? Например: = или ==? - или ->?
    // ────────────────────────────────────────────────────────────────────────────
    switch (c) {
        case '=':  // может быть = или ==
            if (match('=')) return makeToken(TokenType::Operator, "==", startPos);
            return makeToken(TokenType::Operator, "=", startPos);
        case '!':  // может быть ! или !=
            if (match('=')) return makeToken(TokenType::Operator, "!=", startPos);
            return makeToken(TokenType::Operator, "!", startPos);
        case '<':  // может быть < или <=
            if (match('=')) return makeToken(TokenType::Operator, "<=", startPos);
            return makeToken(TokenType::Operator, "<", startPos);
        case '>':  // может быть > или >=
            if (match('=')) return makeToken(TokenType::Operator, ">=", startPos);
            return makeToken(TokenType::Operator, ">", startPos);
        case '&':  // только && поддерживается, одиночный & — ошибка
            if (match('&')) return makeToken(TokenType::Operator, "&&", startPos);
            return errorToken("expected '&' after '&'", startPos);
        case '|':  // только || поддерживается, одиночный | — ошибка
            if (match('|')) return makeToken(TokenType::Operator, "||", startPos);
            return errorToken("expected '|' after '|'", startPos);
        case '-':  // может быть - или -> (стрелка в сигнатуре функции)
            if (match('>')) return makeToken(TokenType::Separator, "->", startPos);
            return makeToken(TokenType::Operator, "-", startPos);
        case ':':  // может быть : или :: (оператор разрешения имён)
            if (match(':')) return makeToken(TokenType::Operator, "::", startPos);
            return makeToken(TokenType::Separator, ":", startPos);
        default:
            break;
    }

    // ────────────────────────────────────────────────────────────────────────────
    // Односимвольные операторы: +, -, *, /, %, .
    // ────────────────────────────────────────────────────────────────────────────
    switch (c) {
        case '+': case '*': case '/': case '%': case '.':
            return makeToken(TokenType::Operator, std::string(1, c), startPos);
        default:
            break;
    }

    // ────────────────────────────────────────────────────────────────────────────
    // Разделители: скобки, точка с запятой, запятая
    // ────────────────────────────────────────────────────────────────────────────
    switch (c) {
        case '(': case ')': case '{': case '}': case '[': case ']':
        case ',': case ';':
            return makeToken(TokenType::Separator, std::string(1, c), startPos);
        default:
            break;
    }

    // Если дошли сюда — это невалидный символ, который лексер не знает обработать
    // Например: ñ, @, #, ~ и другие экзотические символы
    return errorToken("unexpected character '" + std::string(1, c) + "'", startPos);
}

// Получить ВСЕ токены файла сразу (удобнее, чем вызывать nextToken() в цикле)
// Читаем токены до первой ошибки или до EndOfFile
// Если встретили ошибку (токен типа Error), парсер её обработает
std::vector<Token> Lexer::tokenize() {
    std::vector<Token> result;
    while (true) {
        Token token = nextToken();  // читаем следующий токен
        result.push_back(token);
        if (token.type == TokenType::Error || token.type == TokenType::EndOfFile) {
            break;  // стоп: ошибка или конец файла
        }
    }
    return result;
}

// ════════════════════════════════════════════════════════════════════════════════
// Распознать идентификатор или ключевое слово
// ════════════════════════════════════════════════════════════════════════════════
// Алгоритм:
// 1. Первый символ уже прочитан (буква или подчёркивание), index_ указывает на следующий
// 2. Читаем остальные символы: буквы, цифры, подчёркивания
// 3. Проверяем, совпадает ли результат с ключевым словом из таблицы
// 4. Если совпадает → возвращаем Keyword, иначе → Identifier
//
// Примеры:
//   - "let" → Keyword
//   - "x" → Identifier
//   - "myVariable123" → Identifier
Token Lexer::identifierOrKeyword(Position startPos) {
    std::size_t start = index_ - 1;  // index_ уже сдвинут advance(), поэтому start указывает на первый символ
    while (!isAtEnd() && (isAlphaNumeric(peek()) || peek() == '_')) {
        advance();  // читаем буквы, цифры, подчёркивания
    }

    std::string lexeme = source_.substr(start, index_ - start);  // извлекаем текст токена

    // Проверяем булевы литералы отдельно (это ключевые слова, но особого типа)
    if (lexeme == "true" || lexeme == "false") {
        return makeToken(TokenType::BoolLiteral, lexeme, startPos);
    }

    // Таблица зарезервированных ключевых слов
    static const std::unordered_set<std::string> keywords = {
        "module", "namespace", "type", "struct", "fn", "let", "var",
        "if", "else", "while", "break", "continue", "return",
        "as", "unit", "print", "input", "exit", "panic"
    };

    if (keywords.contains(lexeme)) {
        return makeToken(TokenType::Keyword, lexeme, startPos);
    }

    // Иначе это просто Identifier (имя переменной, функции и т.д.)
    return makeToken(TokenType::Identifier, lexeme, startPos);
}

// ════════════════════════════════════════════════════════════════════════════════
// Распознать число: целое или дробное
// ════════════════════════════════════════════════════════════════════════════════
// Алгоритм:
// 1. Читаем все цифры целой части (обязательно)
// 2. Если встретим точку И за ней ЦИФРА → читаем дробную часть → FloatLiteral
// 3. Иначе → IntLiteral
//
// Примеры:
//   - "42" → IntLiteral
//   - "3.14" → FloatLiteral
//   - "10." → IntLiteral (точка без цифры не считается дробью)
Token Lexer::number(Position startPos) {
    std::size_t start = index_ - 1;  // запомнили начало числа
    
    // Целая часть: читаем все цифры
    while (!isAtEnd() && isDigit(peek())) {
        advance();
    }

    // Проверяем наличие дробной части: точка, потом ЦИФРА
    if (!isAtEnd() && peek() == '.' && isDigit(peekNext())) {
        advance();  // пропускаем точку
        // Дробная часть: читаем цифры после точки
        while (!isAtEnd() && isDigit(peek())) {
            advance();
        }
        // Число с точкой → это float
        return makeToken(TokenType::FloatLiteral, source_.substr(start, index_ - start), startPos);
    }

    // Число без точки → это целое
    return makeToken(TokenType::IntLiteral, source_.substr(start, index_ - start), startPos);
}

// ════════════════════════════════════════════════════════════════════════════════
// Распознать строковый литерал: "hello world"
// ════════════════════════════════════════════════════════════════════════════════
// Алгоритм:
// 1. Первая кавычка уже прочитана, начинаем читать содержимое
// 2. Если видим \\ → это escape-последовательность: \\n, \\t, \\", \\\\
// 3. Если видим закрывающую " → готово, StringLiteral
// 4. Если видим перевод строки → ошибка (многострочные строки не допускаются)
// 5. Если дошли до конца файла → ошибка (незакрытая строка)
//
// Поддерживаемые escape-последовательности: \\n (новая строка), \\t (табуляция), \\" (кавычка), \\\\ (бэкслеш)
Token Lexer::stringLiteral(Position startPos) {
    std::string lexeme;
    lexeme.push_back('"');  // сохраняем открывающую кавычку

    while (!isAtEnd()) {
        char c = advance();  // читаем символ
        lexeme.push_back(c);

        if (c == '"') {
            // Нашли закрывающую кавычку → строка завершена
            return makeToken(TokenType::StringLiteral, lexeme, startPos);
        }
        if (c == '\n') {
            // Перевод строки внутри строки → ошибка (строки не многострочные)
            return errorToken("unterminated string literal", startPos);
        }
        if (c == '\\') {
            // Escape-последовательность начинается
            if (isAtEnd()) {
                return errorToken("unterminated string literal", startPos);
            }
            char escaped = advance();  // читаем символ после backslash
            lexeme.push_back(escaped);
            // Проверяем валидные escape-последовательности
            if (escaped != 'n' && escaped != 't' && escaped != '"' && escaped != '\\') {
                return errorToken("invalid escape sequence \\" + std::string(1, escaped), startPos);
            }
        }
    }

    // Дошли до конца файла без закрывающей кавычки → ошибка
    return errorToken("unterminated string literal", startPos);
}

// Форматировать ошибку в стандартном формате: filename:line:column: error: message
// Например: "hello.astra:5:10: error: unexpected character 'ñ'"
// Этот формат IDE может автоматически разобрать и подсветить ошибку красной волнистой линией
std::string Lexer::formatError(const Token& token) const {
    return file_ + ":" + std::to_string(token.pos.line) + ":" +
           std::to_string(token.pos.column) + ": error: " + token.lexeme;
}

// Преобразовать тип токена в строку для красивого вывода
// Используется в режиме отладки --dump-tokens для показа типа каждого распознанного токена
// Пример вывода:
//   5:1   Keyword   let
//   5:5   Identifier  x
//   5:7   Operator  =
//   5:9   IntLiteral  42
std::string tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::Identifier: return "Identifier";      // переменная, функция и т.д.
        case TokenType::Keyword: return "Keyword";            // let, if, fn, while и т.д.
        case TokenType::IntLiteral: return "IntLiteral";       // 42, 123, 0
        case TokenType::FloatLiteral: return "FloatLiteral";   // 3.14, 2.5, 1.0
        case TokenType::StringLiteral: return "StringLiteral"; // "hello", "world"
        case TokenType::BoolLiteral: return "BoolLiteral";     // true, false
        case TokenType::Operator: return "Operator";           // +, -, ==, &&, и т.д.
        case TokenType::Separator: return "Separator";         // (), {}, [], :, ;, , и т.д.
        case TokenType::EndOfFile: return "EndOfFile";         // конец файла
        case TokenType::Error: return "Error";                 // ошибочный токен
    }
    return "Unknown";
}

} // namespace Lexer
