#pragma once

#include "ast.h"

#include <string>
#include <vector>

namespace Codegen {

struct Diagnostic {
    std::string file;
    Lexer::Position pos{};
    std::string message;
};

struct Options {
    std::string outputPath;
    bool buildExecutable = true;
};

// Native x86-64 NASM generator for the checked Astra AST.
// It intentionally lives after Parser and Semantic as an independent compiler phase.
class Generator {
public:
    explicit Generator(std::string fileName = "<input>");

    bool generate(AST::Module& module, const std::string& asmPath);
    const std::vector<Diagnostic>& diagnostics() const noexcept { return diagnostics_; }

private:
    struct Local {
        int offset = 0;
        std::string type;
    };

    struct FunctionContext;

    std::string fileName_;
    std::vector<Diagnostic> diagnostics_;
    std::string asm_;
    std::vector<std::string> namespaceStack_;
    std::vector<std::string> functionLabels_;
    std::vector<std::string> functionFullNames_;
    std::vector<std::string> stringData_;
    int labelCounter_ = 0;

    void addDiagnostic(const AST::Node& node, std::string message);
    void emit(std::string line);
    std::string freshLabel(std::string prefix);
    std::string joinPath(const std::vector<std::string>& path) const;
    std::string currentPrefix() const;
    std::string asmSymbolForPath(const std::vector<std::string>& path) const;
    std::string asmSymbolForQualifiedName(const std::string& name) const;
    std::string stringLabel(const std::string& value);
    static std::string escapeNasmStringBytes(const std::string& value);

    void collectFunctions(AST::Module& module);
    void collectFunctionsInDecl(AST::Decl& decl);
    void collectFunctionLabel(const AST::FunctionDecl& fn);
    void emitModule(AST::Module& module);
    void emitDecl(AST::Decl& decl);
    void emitNamespace(AST::NamespaceDecl& decl);
    void emitFunction(AST::FunctionDecl& fn);

    int countLocalSlots(AST::FunctionDecl& fn) const;
    int countSlotsInBlock(AST::BlockStmt& block) const;
    int countSlotsInStmt(AST::Stmt& stmt) const;

    void emitBlock(AST::BlockStmt& block, FunctionContext& ctx, bool createScope = true);
    void emitStmt(AST::Stmt& stmt, FunctionContext& ctx);
    void emitIf(AST::IfStmt& stmt, FunctionContext& ctx);
    void emitWhile(AST::WhileStmt& stmt, FunctionContext& ctx);
    void emitExpr(AST::Expr& expr, FunctionContext& ctx);
    void emitCall(AST::CallExpr& expr, FunctionContext& ctx);
    void emitBinary(AST::BinaryExpr& expr, FunctionContext& ctx);
    void emitLogicalAnd(AST::BinaryExpr& expr, FunctionContext& ctx);
    void emitLogicalOr(AST::BinaryExpr& expr, FunctionContext& ctx);
    void emitStoreToLValue(AST::Expr& target, FunctionContext& ctx);

    bool isSupportedScalarType(const std::string& type) const;
    std::string exprType(const AST::Expr& expr) const;
};

std::string formatDiagnostic(const Diagnostic& diagnostic);

} // namespace Codegen
