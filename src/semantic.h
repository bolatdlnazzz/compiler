#pragma once

#include "ast.h"
#include "lexer.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Semantic {
struct Diagnostic {
    std::string file;
    Lexer::Position pos{};
    std::string message;
};

struct Type {
    enum class Kind {
        Error,
        Unit,
        Bool,
        Char,
        String,
        Int,
        UInt,
        Float,
        Array,
        Struct
    };
    Kind kind = Kind::Error;
    int bits = 0;
    std::string name;
    std::shared_ptr<Type> elementType;
    std::uint64_t arraySize = 0;
    static Type error();
    static Type unit();
    static Type boolean();
    static Type character();
    static Type string();
    static Type integer(std::string name, int bits, bool isUnsigned);
    static Type floating(std::string name, int bits);
    static Type array(Type element, std::uint64_t size);
    static Type structure(std::string qualifiedName);
    bool operator==(const Type& other) const;
    bool operator!=(const Type& other) const { return !(*this == other); }
    bool isError() const { return kind == Kind::Error; }
    bool isNumeric() const { return kind == Kind::Int || kind == Kind::UInt || kind == Kind::Float; }
    bool isInteger() const { return kind == Kind::Int || kind == Kind::UInt; }
    bool isFloat() const { return kind == Kind::Float; }
    std::string toString() const;
};

class Analyzer {
public:
    explicit Analyzer(std::string fileName = "<input>");
    bool analyze(AST::Module& module);
    const std::vector<Diagnostic>& diagnostics() const noexcept { return diagnostics_; }
private:
    struct Scope;
    struct FunctionInfo;
    struct StructInfo;
    enum class SymbolKind { Variable, Function, Struct, Alias, Namespace };
    struct Symbol {
        SymbolKind kind = SymbolKind::Variable;
        std::string name;
        Type type = Type::error();
        bool isMutable = false;
        std::shared_ptr<FunctionInfo> function;
        std::vector<std::shared_ptr<FunctionInfo>> overloads;
        std::shared_ptr<StructInfo> structure;
        Scope* namespaceScope = nullptr;
    };
    struct Scope {
        Scope* parent = nullptr;
        bool isNamespace = false;
        std::string qualifiedName;
        std::unordered_map<std::string, Symbol> symbols;
    };
    struct FunctionInfo {
        std::string name;
        std::string qualifiedName;
        std::vector<std::string> paramNames;
        std::vector<Type> paramTypes;
        std::vector<AST::Expr*> defaultArgs;
        Type returnType = Type::unit();
        AST::FunctionDecl* decl = nullptr;
        bool isBuiltin = false;
        std::string builtinName;
        std::string codegenName;
    };
    struct StructInfo {
        std::string name;
        std::string qualifiedName;
        std::vector<std::pair<std::string, Type>> fieldsInOrder;
        std::unordered_map<std::string, Type> fields;
    };
    struct LValueInfo {
        Type type = Type::error();
        bool isLValue = false;
        bool isMutable = false;
    };
    enum class Flow { MayContinue, NoContinue };
    std::string fileName_;
    std::vector<Diagnostic> diagnostics_;
    std::vector<std::unique_ptr<Scope>> ownedScopes_;
    Scope* rootScope_ = nullptr;
    Scope* moduleScope_ = nullptr;
    Scope* currentScope_ = nullptr;
    std::vector<std::string> namespaceStack_;
    Type currentReturnType_ = Type::unit();
    bool currentFunctionInfersReturn_ = false;
    bool currentFunctionSawReturnValue_ = false;
    int loopDepth_ = 0;
    Scope* makeScope(Scope* parent, bool isNamespace, std::string qualifiedName = {});
    Scope* ensureNamespace(Scope& scope, const std::string& name, const AST::Node& node);
    std::string qualify(std::string_view name) const;
    std::string joinPath(const std::vector<std::string>& path) const;
    void addDiagnostic(const AST::Node& node, std::string message);
    void addDiagnostic(Lexer::Position pos, std::string message);
    bool hasErrors() const { return !diagnostics_.empty(); }
    void installBuiltins();
    void addBuiltinFunction(std::string name, Type returnType, std::vector<Type> params = {});
    bool declare(Scope& scope, const Symbol& symbol, const AST::Node& node);
    Symbol* lookupLocal(Scope& scope, const std::string& name);
    Symbol* lookupLexical(const std::string& name);
    Symbol* resolvePath(const std::vector<std::string>& path, const AST::Node& node);
    Symbol* resolvePathFromScope(Scope& start, const std::vector<std::string>& path, const AST::Node& node);
    void analyzeDecl(AST::Decl& decl);
    void analyzeNamespaceDecl(AST::NamespaceDecl& decl);
    void analyzeTypeAliasDecl(AST::TypeAliasDecl& decl);
    void analyzeStructDecl(AST::StructDecl& decl);
    void analyzeFunctionDecl(AST::FunctionDecl& decl);
    Type resolveType(AST::TypeExpr& typeExpr);
    Type resolveNamedType(AST::NamedType& typeExpr);
    Flow analyzeBlock(AST::BlockStmt& block, bool createScope);
    Flow analyzeStmt(AST::Stmt& stmt);
    Flow analyzeIf(AST::IfStmt& stmt);
    Flow analyzeWhile(AST::WhileStmt& stmt);
    Flow analyzeExprStmt(AST::ExprStmt& stmt);
    Type analyzeExpr(AST::Expr& expr, const std::optional<Type>& expected = std::nullopt);
    Type analyzeNameExpr(AST::NameExpr& expr);
    Type analyzeArrayLiteral(AST::ArrayLiteralExpr& expr, const std::optional<Type>& expected);
    Type analyzeStructLiteral(AST::StructLiteralExpr& expr);
    Type analyzeUnary(AST::UnaryExpr& expr);
    Type analyzeBinary(AST::BinaryExpr& expr);
    Type analyzeCast(AST::CastExpr& expr);
    Type analyzeSizeOf(AST::SizeOfExpr& expr);
    Type analyzeTypeId(AST::TypeIdExpr& expr);
    Type analyzeIfExpr(AST::IfExpr& expr, const std::optional<Type>& expected);
    Type analyzeCall(AST::CallExpr& expr);
    Type analyzeField(AST::FieldExpr& expr);
    Type analyzeIndex(AST::IndexExpr& expr);
    LValueInfo analyzeLValue(AST::Expr& expr);
    bool checkAssignable(const Type& lhs, const Type& rhs, const AST::Node& node);
    int implicitConversionScore(const Type& lhs, const Type& rhs) const;
    bool canAssignSilently(const Type& lhs, const Type& rhs) const;
    bool canCast(const Type& from, const Type& to) const;
    std::string mangleFunctionName(const std::string& qualifiedName, const std::vector<Type>& params) const;
    Symbol* lookupMethod(const std::string& receiverType, const std::string& methodName);
    bool isPrintable(const Type& type) const;
    bool isTerminatingCall(const AST::Expr& expr) const;
};

std::string formatDiagnostic(const Diagnostic& diagnostic);
}
