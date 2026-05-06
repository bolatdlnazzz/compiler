#include <cctype>
#include <utility>

namespace Astra {

    Lexer::Lexer(std::string_view src, std::string filename)
        : source(src), file(std::move(filename)) {}

    void Lexer::reset() {
        index = 0;
        line = 1;
        column = 1;
    }

    const std::string& Lexer::filename() const {
        return file;
    }

    char Lexer::peek() const {
        if (isAtEnd()) return '\0';
        return source[index];
    }

    char Lexer::peekNext() const {
        if (index + 1 >= source.size()) return '\0';
        return source[index + 1];
    }

    char Lexer::advance() {
        if (isAtEnd()) return '\0';

        char c = source[index++];

        if (c == '\n') {
            line++;
            column = 1;
        } else {
            column++;
        }

        return c;
    }

    bool Lexer::match(char expected) {
        if (isAtEnd()) return false;
        if (source[index] != expected) return false;

        advance();
        return true;
    }

    bool Lexer::isAtEnd() const {
        return index >= source.size();
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

        Position startPos{line, column};

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
            return string(startPos);
        }

        // Двухсимвольные операторы и разделители
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
        }

        // Односимвольные операторы
        switch (c) {
            case '+':
            case '*':
            case '/':
            case '%':
            case '.':
                return makeToken(TokenType::Operator, std::string(1, c), startPos);
        }

        // Разделители
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
        }

        return errorToken("unexpected character '" + std::string(1, c) + "'", startPos);
    }

    Token Lexer::identifierOrKeyword(Position startPos) {
        std::size_t start = index - 1;

        while (!isAtEnd() && (isAlphaNumeric(peek()) || peek() == '_')) {
            advance();
        }

        std::string lexeme = source.substr(start, index - start);

        if (lexeme == "true" || lexeme == "false") {
            return makeToken(TokenType::BoolLiteral, lexeme, startPos);
        }

        static const std::unordered_set<std::string> keywords = {
            "namespace", "type", "struct", "fn", "let", "var",
            "if", "else", "while", "break", "continue", "return",
            "as", "unit", "print", "input", "exit", "panic"
        };

        if (keywords.contains(lexeme)) {
            return makeToken(TokenType::Keyword, lexeme, startPos);
        }

        return makeToken(TokenType::Identifier, lexeme, startPos);
    }

    Token Lexer::number(Position startPos) {
        std::size_t start = index - 1;

        while (!isAtEnd() && isDigit(peek())) {
            advance();
        }

        if (!isAtEnd() && peek() == '.' && isDigit(peekNext())) {
            advance();

            while (!isAtEnd() && isDigit(peek())) {
                advance();
            }

            return makeToken(TokenType::FloatLiteral, source.substr(start, index - start), startPos);
        }

        return makeToken(TokenType::IntLiteral, source.substr(start, index - start), startPos);
    }

    Token Lexer::string(Position startPos) {
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
        return file + ":" + std::to_string(token.pos.line) + ":" +
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

}


// ============================================================
// Пример проверки. Можно вынести в main.cpp
// ============================================================
#ifdef ASTRA_LEXER_TEST
#include <iostream>

int main() {
    std::string code = R"(
namespace Test {
    fn main() -> int32 {
        let x: int32 = 10;
        let y = 3.14;
        let ok = true;
        if (x >= 10 && ok != false) {
            print("hello\nASTRA");
        }
        return 0;
    }
}
)";

    Astra::Lexer lexer(code, "test.astra");

    while (true) {
        Astra::Token token = lexer.nextToken();

        if (token.type == Astra::TokenType::Error) {
            std::cerr << lexer.formatError(token) << '\n';
            return 1;
        }

        std::cout << token.pos.line << ":" << token.pos.column
                  << "  " << Astra::tokenTypeToString(token.type)
                  << "  " << token.lexeme << '\n';

        if (token.type == Astra::TokenType::EndOfFile) {
            break;
        }
    }

    return 0;
}
#endif
