#pragma once
#include "Ast.h"
#include "Lexer.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace Parser {

struct Diagnostic {
    std::string file;
    Lexer::Position pos;
    std::string message;
};

struct Options {
    bool recoverErrors = false;   // по ТЗ базовый режим = false
};

class Parser {
public:
    Parser(std::vector<Lexer::Token> tokens,
           std::string fileName,
           Options options = {});

    std::unique_ptr<AST::Module> parseModule();
    const std::vector<Diagnostic>& diagnostics() const noexcept { return diagnostics_; }

private:
    struct ParseError : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    // Поток токенов
    std::vector<Lexer::Token> tokens_;
    std::string fileName_;
    Options options_{};
    std::vector<Diagnostic> diagnostics_;
    std::size_t current_ = 0;

    // Примитивы
    const Lexer::Token& peek(std::size_t lookahead = 0) const;
    const Lexer::Token& previous() const;
    bool isAtEnd() const;
    const Lexer::Token& advance();

    bool check(Lexer::TokenType type, std::string_view lexeme = {}) const;
    bool match(Lexer::TokenType type, std::string_view lexeme = {});
    const Lexer::Token& expect(Lexer::TokenType type,
                               std::string_view lexeme,
                               std::string_view message);

    [[noreturn]] void errorAt(const Lexer::Token& tok, std::string_view message);
    void synchronizeTopLevel();
    void synchronizeStatement();

    // Helpers
    bool isNameLike(const Lexer::Token& tok) const;
    bool isTypeNameLike(const Lexer::Token& tok) const;
    std::string expectIdentifierLike(std::string_view message);
    std::vector<std::string> parseNamePath();
    AST::SourceSpan spanFrom(const Lexer::Token& first, const Lexer::Token& last) const;

    // Declarations
    AST::DeclPtr parseDeclaration();
    std::unique_ptr<AST::NamespaceDecl> parseNamespaceDecl();
    std::unique_ptr<AST::TypeAliasDecl> parseTypeAliasDecl();
    std::unique_ptr<AST::StructDecl> parseStructDecl();
    std::unique_ptr<AST::FunctionDecl> parseFunctionDecl();

    // Types
    AST::TypePtr parseTypeExpr();

    // Statements
    AST::StmtPtr parseStatement();
    std::unique_ptr<AST::BlockStmt> parseBlock();
    AST::StmtPtr parseLetStmt();
    AST::StmtPtr parseVarStmt();
    AST::StmtPtr parseIfStmt();
    AST::StmtPtr parseWhileStmt();
    AST::StmtPtr parseReturnStmt();

    // Expressions
    AST::ExprPtr parseExpression(int minPrec = 1);
    AST::ExprPtr parseUnary();
    AST::ExprPtr parsePostfix();
    AST::ExprPtr parsePrimary();

    // Predicates for lvalue / precedence
    bool isAssignable(const AST::Expr& expr) const;
    int precedenceOf(const Lexer::Token& tok) const;
    bool isLeftAssociative(const Lexer::Token& tok) const;
};

} // namespace Parser
