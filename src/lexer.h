#pragma once

#include <string>
#include <string_view>
#include <unordered_set>

namespace Astra {

    enum class TokenType {
        Identifier,
        Keyword,
        IntLiteral,
        FloatLiteral,
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
        TokenType type;
        std::string lexeme;
        Position pos;
    };

    class Lexer {
    public:
        explicit Lexer(std::string_view source, std::string filename = "<input>");

        Token nextToken();
        void reset();

        const std::string& filename() const;
        std::string formatError(const Token& token) const;

    private:
        std::string source;
        std::string file;
        std::size_t index = 0;
        int line = 1;
        int column = 1;

        char peek() const;
        char peekNext() const;
        char advance();
        bool match(char expected);
        bool isAtEnd() const;

        Token makeToken(TokenType type, const std::string& lexeme, Position startPos) const;
        Token errorToken(const std::string& message, Position startPos) const;

        Token identifierOrKeyword(Position startPos);
        Token number(Position startPos);
        Token string(Position startPos);

        void skipWhitespace();
        void skipComment();
        void skipWhitespaceAndComments();

        static bool isAlpha(char c);
        static bool isDigit(char c);
        static bool isAlphaNumeric(char c);
    };

    std::string tokenTypeToString(TokenType type);

}
