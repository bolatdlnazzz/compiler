#include "lexer.h"

#include <cctype>
#include <string>
#include <unordered_set>
#include <utility>

using Token = Lexer::Token;
using TokenType = Lexer::TokenType;
using Position = Lexer::Position;

Lexer::Lexer::Lexer(std::string_view src, std::string filename) : source_(src), file_(std::move(filename)) {}

void Lexer::Lexer::reset() {
    index_ = 0;
    line_ = 1;
    column_ = 1;
}

const std::string& Lexer::Lexer::filename() const {
    return file_;
}

char Lexer::Lexer::peek() const {
    if (isAtEnd()) {
        return '\0';
    }
    return source_[index_];
}

char Lexer::Lexer::peekNext() const {
    if (index_ + 1 >= source_.size()) {
        return '\0';
    }
    return source_[index_ + 1];
}

char Lexer::Lexer::advance() {
    if (isAtEnd()) {
        return '\0';
    }
    char c = source_[index_++];
    if (c == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }
    return c;
}

bool Lexer::Lexer::match(char expected) {
    if (isAtEnd() || source_[index_] != expected) {
        return false;
    }
    advance();
    return true;
}

bool Lexer::Lexer::isAtEnd() const {
    return index_ >= source_.size();
}

Token Lexer::Lexer::makeToken(TokenType type, const std::string& lexeme, Position startPos) const {
    return Token{type, lexeme, startPos};
}

Token Lexer::Lexer::errorToken(const std::string& message, Position startPos) const {
    return Token{TokenType::Error, message, startPos};
}

bool Lexer::Lexer::isAlpha(char c) {
    unsigned char uc = static_cast<unsigned char>(c);
    return std::isalpha(uc) != 0;
}

bool Lexer::Lexer::isDigit(char c) {
    unsigned char uc = static_cast<unsigned char>(c);
    return std::isdigit(uc) != 0;
}

bool Lexer::Lexer::isHexDigit(char c) {
    unsigned char uc = static_cast<unsigned char>(c);
    return std::isxdigit(uc) != 0;
}

bool Lexer::Lexer::isAlphaNumeric(char c) {
    return isAlpha(c) || isDigit(c);
}

void Lexer::Lexer::skipWhitespace() {
    while (!isAtEnd()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else {
            break;
        }
    }
}

void Lexer::Lexer::skipComment() {
    while (!isAtEnd() && peek() != '\n') {
        advance();
    }
}

void Lexer::Lexer::skipWhitespaceAndComments() {
    while (!isAtEnd()) {
        skipWhitespace();
        if (peek() == '/' && peekNext() == '/') {
            skipComment();
            continue;
        }
        break;
    }
}

Token Lexer::Lexer::nextToken() {
    skipWhitespaceAndComments();
    Position startPos{line_, column_};
    if (isAtEnd()) {
        return makeToken(TokenType::EndOfFile, "", startPos);
    }
    char c = advance();
    if (isAlpha(c) || c == '_') {
        return identifierOrKeyword(startPos);
    }
    if (isDigit(c)) {
        return number(startPos);
    }
    if (c == '"') {
        return stringLiteral(startPos);
    }
    if (c == '\'') {
        return charLiteral(startPos);
    }

    switch (c) {
        case '=':
            if (match('=')) {
                return makeToken(TokenType::Operator, "==", startPos);
            }
            return makeToken(TokenType::Operator, "=", startPos);
        case '!':
            if (match('=')) {
                return makeToken(TokenType::Operator, "!=", startPos);
            }
            return makeToken(TokenType::Operator, "!", startPos);
        case '<':
            if (match('=')) {
                return makeToken(TokenType::Operator, "<=", startPos);
            }
            return makeToken(TokenType::Operator, "<", startPos);
        case '>':
            if (match('=')) {
                return makeToken(TokenType::Operator, ">=", startPos);
            }
            return makeToken(TokenType::Operator, ">", startPos);
        case '&':
            if (match('&')) {
                return makeToken(TokenType::Operator, "&&", startPos);
            }
            return errorToken("expected '&' after '&'", startPos);
        case '|':
            if (match('|')) {
                return makeToken(TokenType::Operator, "||", startPos);
            }
            return errorToken("expected '|' after '|'", startPos);
        case '-':
            if (match('>')) {
                return makeToken(TokenType::Separator, "->", startPos);
            }
            return makeToken(TokenType::Operator, "-", startPos);
        case ':':
            if (match(':')) {
                return makeToken(TokenType::Operator, "::", startPos);
            }
            return makeToken(TokenType::Separator, ":", startPos);
        default:
            break;
    }

    switch (c) {
        case '+':
        case '*':
        case '/':
        case '%':
        case '.':
            return makeToken(TokenType::Operator, std::string(1, c), startPos);
        default:
            break;
    }

    switch (c) {
        case '(':
        case ')':
        case '{':
        case '}':
        case '[':
        case ']':
        case ',':
        case ';':
            return makeToken(TokenType::Separator, std::string(1, c), startPos);
        default:
            break;
    }

    return errorToken("unexpected character '" + std::string(1, c) + "'", startPos);
}

std::vector<Token> Lexer::Lexer::tokenize() {
    std::vector<Token> result;
    while (true) {
        Token token = nextToken();
        result.push_back(token);
        if (token.type == TokenType::Error || token.type == TokenType::EndOfFile) {
            break;
        }
    }
    return result;
}

Token Lexer::Lexer::identifierOrKeyword(Position startPos) {
    std::size_t start = index_ - 1;
    while (!isAtEnd() && (isAlphaNumeric(peek()) || peek() == '_')) {
        advance();
    }

    std::string lexeme = source_.substr(start, index_ - start);
    if (lexeme == "true" || lexeme == "false") {
        return makeToken(TokenType::BoolLiteral, lexeme, startPos);
    }
    if (lexeme == "inf" || lexeme == "NaN" || lexeme == "nan") {
        return makeToken(TokenType::FloatLiteral, lexeme == "nan" ? std::string("NaN") : lexeme, startPos);
    }
    static const std::unordered_set<std::string> keywords = {
        "module", "namespace", "type", "struct", "fn", "let", "var",
        "if", "else", "while", "break", "continue", "return",
        "as", "unit", "print", "input", "exit", "panic", "assert"
    };

    if (keywords.contains(lexeme)) {
        return makeToken(TokenType::Keyword, lexeme, startPos);
    }

    return makeToken(TokenType::Identifier, lexeme, startPos);
}

Token Lexer::Lexer::number(Position startPos) {
    std::size_t start = index_ - 1;
    if (source_[start] == '0' && (peek() == 'x' || peek() == 'X')) {
        advance();
        if (isAtEnd() || !isHexDigit(peek())) {
            return errorToken("expected hexadecimal digits after 0x", startPos);
        }
        while (!isAtEnd() && isHexDigit(peek())) {
            advance();
        }
        return makeToken(TokenType::IntLiteral, source_.substr(start, index_ - start), startPos);
    }

    if (source_[start] == '0' && (peek() == 'b' || peek() == 'B')) {
        advance();
        if (isAtEnd() || (peek() != '0' && peek() != '1')) {
            return errorToken("expected binary digits after 0b", startPos);
        }
        while (!isAtEnd() && (peek() == '0' || peek() == '1')) {
            advance();
        }
        return makeToken(TokenType::IntLiteral, source_.substr(start, index_ - start), startPos);
    }
    bool isFloat = false;
    while (!isAtEnd() && isDigit(peek())) {
        advance();
    }

    if (!isAtEnd() && peek() == '.' && isDigit(peekNext())) {
        isFloat = true;
        advance();
        while (!isAtEnd() && isDigit(peek())) {
            advance();
        }
    }

    if (!isAtEnd() && (peek() == 'e' || peek() == 'E')) {
        isFloat = true;
        advance();
        if (!isAtEnd() && (peek() == '+' || peek() == '-')) {
            advance();
        }
        if (isAtEnd() || !isDigit(peek())) {
            return errorToken("expected exponent digits in float literal", startPos);
        }
        while (!isAtEnd() && isDigit(peek())) {
            advance();
        }
    }
    return makeToken(
        isFloat ? TokenType::FloatLiteral : TokenType::IntLiteral,
        source_.substr(start, index_ - start),
        startPos);
}

Token Lexer::Lexer::stringLiteral(Position startPos) {
    std::string lexeme;
    lexeme.push_back('"');
    while (!isAtEnd()) {
        char c = advance();
        lexeme.push_back(c);
        if (c == '"') {
            return makeToken(TokenType::StringLiteral, lexeme, startPos);
        }
        if (c == '\n') {
            return errorToken("unterminated string literal", startPos);
        }
        if (c == '\\') {
            if (isAtEnd()) {
                return errorToken("unterminated string literal", startPos);
            }
            char escaped = advance();
            lexeme.push_back(escaped);
            if (escaped != 'n' && escaped != 't' && escaped != '"' && escaped != '\\') {
                return errorToken("invalid escape sequence \\" + std::string(1, escaped), startPos);
            }
        }
    }
    return errorToken("unterminated string literal", startPos);
}

Token Lexer::Lexer::charLiteral(Position startPos) {
    std::string lexeme;
    lexeme.push_back('\'');
    if (isAtEnd() || peek() == '\n') {
        return errorToken("unterminated char literal", startPos);
    }
    char c = advance();
    lexeme.push_back(c);
    if (c == '\\') {
        if (isAtEnd() || peek() == '\n') {
            return errorToken("unterminated char literal", startPos);
        }
        char escaped = advance();
        lexeme.push_back(escaped);
        if (escaped != 'n' && escaped != 't' && escaped != '\'' && escaped != '\\' && escaped != '0') {
            return errorToken("invalid escape sequence \\" + std::string(1, escaped), startPos);
        }
    }

    if (!match('\'')) {
        return errorToken("char literal must contain exactly one character", startPos);
    }
    lexeme.push_back('\'');
    return makeToken(TokenType::CharLiteral, lexeme, startPos);
}

std::string Lexer::Lexer::formatError(const Token& token) const {
    return file_ + ":" + std::to_string(token.pos.line) + ":" +
           std::to_string(token.pos.column) + ": error: " + token.lexeme;
}

std::string Lexer::tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::Identifier: return "Identifier";
        case TokenType::Keyword: return "Keyword";
        case TokenType::IntLiteral: return "IntLiteral";
        case TokenType::FloatLiteral: return "FloatLiteral";
        case TokenType::CharLiteral: return "CharLiteral";
        case TokenType::StringLiteral: return "StringLiteral";
        case TokenType::BoolLiteral: return "BoolLiteral";
        case TokenType::Operator: return "Operator";
        case TokenType::Separator: return "Separator";
        case TokenType::EndOfFile: return "EndOfFile";
        case TokenType::Error: return "Error";
    }
    return "Unknown";
}
