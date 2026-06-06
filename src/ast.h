#pragma once
#include "lexer.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace AST {
struct SourceSpan {
    std::string file;
    Lexer::Position begin{};
    Lexer::Position end{};
};

struct Node {
    SourceSpan span;
    virtual ~Node() = default;
};

struct TypeExpr : Node {
    virtual ~TypeExpr() = default;
};

struct Expr : Node {
    std::optional<std::string> semanticType;
    virtual ~Expr() = default;
};

struct Stmt : Node {
    virtual ~Stmt() = default;
};

struct Decl : Node {
    virtual ~Decl() = default;
};

using TypePtr = std::unique_ptr<TypeExpr>;
using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;
using DeclPtr = std::unique_ptr<Decl>;
struct Module final : Node {
    std::vector<std::string> namePath;
    std::vector<DeclPtr> decls;
};

struct NamedType final : TypeExpr {
    std::vector<std::string> path;
};

struct ArrayType final : TypeExpr {
    TypePtr elementType;
    std::uint64_t size = 0;
};

struct NameExpr final : Expr {
    std::vector<std::string> path;
};

struct IntLiteralExpr final : Expr {
    std::string lexeme;
};

struct FloatLiteralExpr final : Expr {
    std::string lexeme;
};

struct BoolLiteralExpr final : Expr {
    bool value = false;
};

struct CharLiteralExpr final : Expr {
    char value = '\0';
};

struct StringLiteralExpr final : Expr {
    std::string value;
};

struct ArrayLiteralExpr final : Expr {
    std::vector<ExprPtr> elements;
};

struct StructFieldInit {
    std::string name;
    ExprPtr value;
    SourceSpan span;
};

struct StructLiteralExpr final : Expr {
    std::vector<std::string> typePath;
    std::vector<StructFieldInit> fields;
};

struct UnaryExpr final : Expr {
    std::string op;
    ExprPtr operand;
};

struct BinaryExpr final : Expr {
    std::string op;
    ExprPtr left;
    ExprPtr right;
};

struct CastExpr final : Expr {
    ExprPtr value;
    TypePtr targetType;
};

struct CallExpr final : Expr {
    ExprPtr callee;
    std::vector<ExprPtr> args;
};

struct FieldExpr final : Expr {
    ExprPtr object;
    std::string field;
};

struct IndexExpr final : Expr {
    ExprPtr object;
    ExprPtr index;
};

struct EmptyStmt final : Stmt {};
struct BlockStmt final : Stmt {
    std::vector<StmtPtr> statements;
};

struct LetStmt final : Stmt {
    std::string name;
    TypePtr explicitType;
    ExprPtr initializer;
};

struct VarStmt final : Stmt {
    std::string name;
    TypePtr explicitType;
    ExprPtr initializer;
};

struct AssignStmt final : Stmt {
    ExprPtr target;
    ExprPtr value;
};

struct ExprStmt final : Stmt {
    ExprPtr expr;
};

struct IfStmt final : Stmt {
    ExprPtr condition;
    std::unique_ptr<BlockStmt> thenBlock;
    StmtPtr elseBranch;
};

struct WhileStmt final : Stmt {
    ExprPtr condition;
    std::unique_ptr<BlockStmt> body;
};

struct BreakStmt final : Stmt {};
struct ContinueStmt final : Stmt {};
struct ReturnStmt final : Stmt {
    ExprPtr value;
};

struct Param {
    std::string name;
    TypePtr type;
    SourceSpan span;
};

struct FieldDecl {
    std::string name;
    TypePtr type;
    SourceSpan span;
};

struct NamespaceDecl final : Decl {
    std::string name;
    std::vector<DeclPtr> decls;
};

struct TypeAliasDecl final : Decl {
    std::string name;
    TypePtr aliasedType;
};

struct StructDecl final : Decl {
    std::string name;
    std::vector<FieldDecl> fields;
};

struct FunctionDecl final : Decl {
    std::string name;
    std::vector<Param> params;
    TypePtr returnType;
    std::unique_ptr<BlockStmt> body;
};
}
