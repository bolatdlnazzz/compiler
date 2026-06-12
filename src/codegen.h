#pragma once

#include "ast.h"

#include <cstdint>
#include <string>
#include <unordered_map>
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

class Generator {
public:
    explicit Generator(std::string fileName = "<input>");
    bool generate(AST::Module& module, const std::string& asmPath);
    const std::vector<Diagnostic>& diagnostics() const noexcept { return diagnostics_; }
private:
    struct Local {
        int offset = 0;
        int size = 0;
        int align = 1;
        std::string type;
    };
    struct FieldLayout {
        std::string name;
        std::string type;
        int offset = 0;
        int size = 0;
        int align = 1;
    };
    struct StructLayout {
        std::string name;
        int size = 0;
        int align = 1;
        std::vector<FieldLayout> fields;
        std::unordered_map<std::string, std::size_t> indexByName;
    };
    struct ArrayInfo {
        std::string elementType;
        std::uint64_t size = 0;
        bool valid = false;
    };
    struct FunctionContext;
    std::string fileName_;
    std::vector<Diagnostic> diagnostics_;
    std::string asm_;
    std::vector<std::string> namespaceStack_;
    std::vector<std::string> functionFullNames_;
    std::vector<std::string> stringData_;
    std::vector<std::string> floatData_;
    std::unordered_map<std::string, StructLayout> structs_;
    std::unordered_map<std::string, std::string> aliases_;
    int labelCounter_ = 0;
    void addDiagnostic(const AST::Node& node, std::string message);
    void emit(std::string line);
    std::string freshLabel(std::string prefix);
    std::string joinPath(const std::vector<std::string>& path) const;
    std::string currentPrefix() const;
    std::string asmSymbolForQualifiedName(const std::string& name) const;
    std::string asmSymbolForPath(const std::vector<std::string>& path) const;
    std::string stringLabel(const std::string& value);
    std::string floatLabel(const std::string& value, const std::string& type);
    static std::string escapeNasmStringBytes(const std::string& value);
    static std::string trim(std::string value);
    static int alignTo(int value, int alignment);
    static bool startsWith(const std::string& value, const std::string& prefix);
    void collectSymbols(AST::Module& module);
    void collectSymbolsInDecl(AST::Decl& decl);
    void collectFunctionLabel(const AST::FunctionDecl& fn);
    void collectStructLayout(const AST::StructDecl& st);
    void collectAlias(const AST::TypeAliasDecl& alias);
    void emitModule(AST::Module& module);
    void emitDecl(AST::Decl& decl);
    void emitNamespace(AST::NamespaceDecl& decl);
    void emitFunction(AST::FunctionDecl& fn);
    std::string typeExprToString(const AST::TypeExpr& typeExpr) const;
    std::string resolveNamedType(const std::vector<std::string>& path) const;
    std::string exprType(const AST::Expr& expr) const;
    bool isFloatType(const std::string& type) const;
    bool isSignedIntType(const std::string& type) const;
    bool isUnsignedIntType(const std::string& type) const;
    bool isIntegerType(const std::string& type) const;
    bool isBoolType(const std::string& type) const;
    bool isStringType(const std::string& type) const;
    bool isScalarType(const std::string& type) const;
    bool isAggregateType(const std::string& type) const;
    int typeSize(const std::string& type) const;
    int typeAlign(const std::string& type) const;
    ArrayInfo parseArrayType(const std::string& type) const;
    const StructLayout* findStruct(const std::string& type) const;
    const FieldLayout* findField(const std::string& type, const std::string& field) const;
    int computeFrameSize(AST::FunctionDecl& fn);
    void scanBlockForFrame(AST::BlockStmt& block, FunctionContext& ctx);
    void scanStmtForFrame(AST::Stmt& stmt, FunctionContext& ctx);
    void emitBlock(AST::BlockStmt& block, FunctionContext& ctx, bool createScope = true);
    void emitStmt(AST::Stmt& stmt, FunctionContext& ctx);
    void emitIf(AST::IfStmt& stmt, FunctionContext& ctx);
    void emitWhile(AST::WhileStmt& stmt, FunctionContext& ctx);
    void emitExpr(AST::Expr& expr, FunctionContext& ctx);
    void emitCall(AST::CallExpr& expr, FunctionContext& ctx);
    void emitIfExpr(AST::IfExpr& expr, FunctionContext& ctx);
    void emitBinary(AST::BinaryExpr& expr, FunctionContext& ctx);
    void emitFloatBinary(AST::BinaryExpr& expr, FunctionContext& ctx, const std::string& type);
    void emitLogicalAnd(AST::BinaryExpr& expr, FunctionContext& ctx);
    void emitLogicalOr(AST::BinaryExpr& expr, FunctionContext& ctx);
    void emitCast(AST::CastExpr& expr, FunctionContext& ctx);
    void emitAddressOf(AST::Expr& expr, FunctionContext& ctx);
    void emitStoreToLValue(AST::Expr& target, const std::string& valueType, FunctionContext& ctx);
    void emitInitToAddress(AST::Expr& expr, const std::string& targetType, FunctionContext& ctx);
    void emitCopyBytes(const std::string& dstReg, const std::string& srcReg, int size);
    void emitLoadFromAddress(const std::string& reg, const std::string& type);
    void emitStoreToAddress(const std::string& reg, const std::string& type);
    void emitNormalizeInteger(const std::string& type);
    void emitCallInstruction(FunctionContext& ctx, const std::string& callee);
    void emitCallAggregateDest(AST::Expr& callExpr, const std::string& targetType, FunctionContext& ctx);
    void emitPush(FunctionContext& ctx, const std::string& reg);
    void emitPop(FunctionContext& ctx, const std::string& reg);
    std::string mem(const std::string& baseReg, int offset = 0) const;
    std::string localAddress(const Local& local) const;
};

std::string formatDiagnostic(const Diagnostic& diagnostic);
}
