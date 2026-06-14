#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace Lexer {
enum class TokenType {
    Identifier,
    Keyword,
    IntLiteral,
    FloatLiteral,
    CharLiteral,
    StringLiteral,
    BoolLiteral,
    Operator,
    Separator,
    EndOfFile,
    Error
};

struct Position {
    int line = 1;
    int column = 1;
};

struct Token {
    TokenType type = TokenType::Error;
    std::string lexeme;
    Position pos{};
};

class Lexer {
public:
    explicit Lexer(std::string_view source, std::string filename = "<input>");
    Token nextToken();
    std::vector<Token> tokenize();
    void reset();
    const std::string& filename() const;
    std::string formatError(const Token& token) const;
private:
    std::string source_;
    std::string file_;
    std::size_t index_ = 0;
    int line_ = 1;
    int column_ = 1;
    bool pendingBlockCommentError_ = false;
    Position pendingBlockCommentErrorPos_{};
    char peek() const;
    char peekNext() const;
    char advance();
    bool match(char expected);
    bool isAtEnd() const;
    Token makeToken(TokenType type, const std::string& lexeme, Position startPos) const;
    Token errorToken(const std::string& message, Position startPos) const;
    Token identifierOrKeyword(Position startPos);
    Token number(Position startPos);
    Token stringLiteral(Position startPos);
    Token charLiteral(Position startPos);
    void skipWhitespace();
    void skipComment();
    void skipBlockComment(); //блочные комментарии
    void skipWhitespaceAndComments();
    static bool isAlpha(char c);
    static bool isDigit(char c);
    static bool isHexDigit(char c);
    static bool isAlphaNumeric(char c);
};

std::string tokenTypeToString(TokenType type);
}
