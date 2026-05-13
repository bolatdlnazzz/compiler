#include "lexer.h"

#include <cctype>
#include <string>
#include <unordered_set>
#include <utility>

namespace Lexer {

Lexer::Lexer(std::string_view src, std::string filename)
    : source_(src), file_(std::move(filename)) {}

void Lexer::reset() {
    index_ = 0;
    line_ = 1;
    column_ = 1;
}

const std::string& Lexer::filename() const {
    return file_;
}

char Lexer::peek() const {
    if (isAtEnd()) return '\0';
    return source_[index_];
}

char Lexer::peekNext() const {
    if (index_ + 1 >= source_.size()) return '\0';
    return source_[index_ + 1];
}

char Lexer::advance() {
    if (isAtEnd()) return '\0';

    char c = source_[index_++];
    if (c == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }
    return c;
}

bool Lexer::match(char expected) {
    if (isAtEnd()) return false;
    if (source_[index_] != expected) return false;
    advance();
    return true;
}

bool Lexer::isAtEnd() const {
    return index_ >= source_.size();
}

Token Lexer::makeToken(TokenType type, const std::string& lexeme, Position startPos) const {
    return Token{type, lexeme, startPos};
}

Token Lexer::errorToken(const std::string& message, Position startPos) const {
    return Token{TokenType::Error, message, startPos};
}

bool Lexer::isAlpha(char c) {
    unsigned char uc = static_cast<unsigned char>(c);
    return std::isalpha(uc) != 0;
}

bool Lexer::isDigit(char c) {
    unsigned char uc = static_cast<unsigned char>(c);
    return std::isdigit(uc) != 0;
}

bool Lexer::isAlphaNumeric(char c) {
    return isAlpha(c) || isDigit(c);
}

void Lexer::skipWhitespace() {
    while (!isAtEnd()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else {
            break;
        }
    }
}

void Lexer::skipComment() {
    while (!isAtEnd() && peek() != '\n') {
        advance();
    }
}

void Lexer::skipWhitespaceAndComments() {
    while (!isAtEnd()) {
        skipWhitespace();
        if (peek() == '/' && peekNext() == '/') {
            skipComment();
            continue;
        }
        break;
    }
}

Token Lexer::nextToken() {
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

    switch (c) {
        case '=':
            if (match('=')) return makeToken(TokenType::Operator, "==", startPos);
            return makeToken(TokenType::Operator, "=", startPos);
        case '!':
            if (match('=')) return makeToken(TokenType::Operator, "!=", startPos);
            return makeToken(TokenType::Operator, "!", startPos);
        case '<':
            if (match('=')) return makeToken(TokenType::Operator, "<=", startPos);
            return makeToken(TokenType::Operator, "<", startPos);
        case '>':
            if (match('=')) return makeToken(TokenType::Operator, ">=", startPos);
            return makeToken(TokenType::Operator, ">", startPos);
        case '&':
            if (match('&')) return makeToken(TokenType::Operator, "&&", startPos);
            return errorToken("expected '&' after '&'", startPos);
        case '|':
            if (match('|')) return makeToken(TokenType::Operator, "||", startPos);
            return errorToken("expected '|' after '|'", startPos);
        case '-':
            if (match('>')) return makeToken(TokenType::Separator, "->", startPos);
            return makeToken(TokenType::Operator, "-", startPos);
        case ':':
            if (match(':')) return makeToken(TokenType::Operator, "::", startPos);
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

std::vector<Token> Lexer::tokenize() {
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

Token Lexer::identifierOrKeyword(Position startPos) {
    std::size_t start = index_ - 1;
    while (!isAtEnd() && (isAlphaNumeric(peek()) || peek() == '_')) {
        advance();
    }

    std::string lexeme = source_.substr(start, index_ - start);

    if (lexeme == "true" || lexeme == "false") {
        return makeToken(TokenType::BoolLiteral, lexeme, startPos);
    }

    static const std::unordered_set<std::string> keywords = {
        "module", "namespace", "type", "struct", "fn", "let", "var",
        "if", "else", "while", "break", "continue", "return",
        "as", "unit", "print", "input", "exit", "panic"
    };

    if (keywords.contains(lexeme)) {
        return makeToken(TokenType::Keyword, lexeme, startPos);
    }

    return makeToken(TokenType::Identifier, lexeme, startPos);
}

Token Lexer::number(Position startPos) {
    std::size_t start = index_ - 1;
    while (!isAtEnd() && isDigit(peek())) {
        advance();
    }

    if (!isAtEnd() && peek() == '.' && isDigit(peekNext())) {
        advance();
        while (!isAtEnd() && isDigit(peek())) {
            advance();
        }
        return makeToken(TokenType::FloatLiteral, source_.substr(start, index_ - start), startPos);
    }

    return makeToken(TokenType::IntLiteral, source_.substr(start, index_ - start), startPos);
}

Token Lexer::stringLiteral(Position startPos) {
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

std::string Lexer::formatError(const Token& token) const {
    return file_ + ":" + std::to_string(token.pos.line) + ":" +
           std::to_string(token.pos.column) + ": error: " + token.lexeme;
}

std::string tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::Identifier: return "Identifier";
        case TokenType::Keyword: return "Keyword";
        case TokenType::IntLiteral: return "IntLiteral";
        case TokenType::FloatLiteral: return "FloatLiteral";
        case TokenType::StringLiteral: return "StringLiteral";
        case TokenType::BoolLiteral: return "BoolLiteral";
        case TokenType::Operator: return "Operator";
        case TokenType::Separator: return "Separator";
        case TokenType::EndOfFile: return "EndOfFile";
        case TokenType::Error: return "Error";
    }
    return "Unknown";
}

} // namespace Lexer
