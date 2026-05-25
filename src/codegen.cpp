#include "codegen.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <utility>

namespace Codegen {

// Сначала собираем информацию о типах и символах, потом пишем итоговый asm.
struct Generator::FunctionContext {
    explicit FunctionContext(Generator& g) : gen(g) {}

    Generator& gen;
    std::vector<std::unordered_map<std::string, Local>> scopes;
    std::vector<std::string> breakLabels;
    std::vector<std::string> continueLabels;
    std::string returnLabel;
    int nextOffset = 0;
    int stackBytes = 0;

    void pushScope() { scopes.emplace_back(); }
    void popScope() { scopes.pop_back(); }

    Local addLocal(const std::string& name, std::string type) {
        const int size = std::max(1, gen.typeSize(type));
        const int align = std::max(1, gen.typeAlign(type));
        nextOffset = Generator::alignTo(nextOffset, align);
        nextOffset += size;
        Local local{-nextOffset, size, align, std::move(type)};
        if (scopes.empty()) pushScope();
        scopes.back()[name] = local;
        return local;
    }

    const Local* findLocal(const std::string& name) const {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) return &found->second;
        }
        return nullptr;
    }
};

Generator::Generator(std::string fileName) : fileName_(std::move(fileName)) {}

void Generator::addDiagnostic(const AST::Node& node, std::string message) {
    diagnostics_.push_back(Diagnostic{fileName_, node.span.begin, std::move(message)});
}

void Generator::emit(std::string line) {
    asm_ += std::move(line);
    asm_ += '\n';
}

std::string Generator::freshLabel(std::string prefix) {
    return ".L" + std::move(prefix) + std::to_string(labelCounter_++);
}

std::string Generator::joinPath(const std::vector<std::string>& path) const {
    std::string result;
    for (const auto& part : path) {
        if (!result.empty()) result += "::";
        result += part;
    }
    return result;
}

std::string Generator::currentPrefix() const {
    std::string result;
    for (const auto& part : namespaceStack_) {
        if (!result.empty()) result += "::";
        result += part;
    }
    return result;
}

std::string Generator::asmSymbolForQualifiedName(const std::string& name) const {
    if (name == "main") return "main";
    std::string result = "astra_";
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c))) result += c;
        else result += '_';
    }
    return result;
}

std::string Generator::asmSymbolForPath(const std::vector<std::string>& path) const {
    if (path.empty()) return {};
    if (path.size() == 1) {
        std::string qualified = path[0];
        if (!namespaceStack_.empty()) {
            const std::string candidate = currentPrefix() + "::" + path[0];
            if (std::find(functionFullNames_.begin(), functionFullNames_.end(), candidate) != functionFullNames_.end()) {
                qualified = candidate;
            }
        }
        return asmSymbolForQualifiedName(qualified);
    }
    return asmSymbolForQualifiedName(joinPath(path));
}

std::string Generator::trim(std::string value) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

int Generator::alignTo(int value, int alignment) {
    if (alignment <= 1) return value;
    const int rem = value % alignment;
    return rem == 0 ? value : value + (alignment - rem);
}

bool Generator::startsWith(const std::string& value, const std::string& prefix) {
    return value.rfind(prefix, 0) == 0;
}

std::string Generator::escapeNasmStringBytes(const std::string& value) {
    std::ostringstream out;
    bool first = true;
    auto comma = [&]() {
        if (!first) out << ", ";
        first = false;
    };
    std::string chunk;
    auto flushChunk = [&]() {
        if (chunk.empty()) return;
        comma();
        out << '"';
        for (char c : chunk) {
            if (c == '"' || c == '\\') out << '\\';
            out << c;
        }
        out << '"';
        chunk.clear();
    };

    for (unsigned char c : value) {
        if (c >= 32 && c <= 126 && c != '"' && c != '\\') {
            chunk.push_back(static_cast<char>(c));
        } else if (c == '"' || c == '\\') {
            chunk.push_back(static_cast<char>(c));
        } else {
            flushChunk();
            comma();
            out << static_cast<int>(c);
        }
    }
    flushChunk();
    comma();
    out << 0;
    return out.str();
}

std::string Generator::stringLabel(const std::string& value) {
    const std::string label = "LC" + std::to_string(stringData_.size());
    stringData_.push_back(label + ": db " + escapeNasmStringBytes(value));
    return label;
}

std::string Generator::floatLabel(const std::string& value, const std::string& type) {
    const std::string label = "LF" + std::to_string(floatData_.size());
    floatData_.push_back(label + (type == "float32" ? ": dd " : ": dq ") + value);
    return label;
}

std::string Generator::mem(const std::string& baseReg, int offset) const {
    if (offset == 0) return "[" + baseReg + "]";
    if (offset > 0) return "[" + baseReg + " + " + std::to_string(offset) + "]";
    return "[" + baseReg + " - " + std::to_string(-offset) + "]";
}

std::string Generator::localAddress(const Local& local) const {
    return "[rbp - " + std::to_string(-local.offset) + "]";
}

bool Generator::generate(AST::Module& module, const std::string& asmPath) {
    diagnostics_.clear();
    asm_.clear();
    stringData_.clear();
    floatData_.clear();
    functionFullNames_.clear();
    namespaceStack_.clear();
    structs_.clear();
    aliases_.clear();
    labelCounter_ = 0;

    collectSymbols(module);
    namespaceStack_.clear();
    emitModule(module);

    if (!diagnostics_.empty()) return false;

    std::string finalAsm;
    finalAsm += "default rel\n";
    finalAsm += "section .data\n";
    for (const auto& line : stringData_) finalAsm += line + "\n";
    for (const auto& line : floatData_) finalAsm += line + "\n";
    finalAsm += "section .text\n";
    finalAsm += "global main\n";
    finalAsm += "extern astra_print_i32\n";
    finalAsm += "extern astra_print_i64\n";
    finalAsm += "extern astra_print_u32\n";
    finalAsm += "extern astra_print_u64\n";
    finalAsm += "extern astra_print_f32\n";
    finalAsm += "extern astra_print_f64\n";
    finalAsm += "extern astra_print_bool\n";
    finalAsm += "extern astra_print_string\n";
    finalAsm += "extern astra_input_string\n";
    finalAsm += "extern astra_exit\n";
    finalAsm += "extern astra_panic\n";
    finalAsm += "extern astra_string_concat\n";
    finalAsm += "extern astra_string_eq\n";
    finalAsm += "extern astra_string_ne\n";
    finalAsm += "extern astra_string_len\n";
    finalAsm += "extern astra_rt_div_zero\n";
    finalAsm += "extern astra_rt_oob\n";
    finalAsm += asm_;

    std::ofstream out(asmPath);
    if (!out) {
        diagnostics_.push_back(Diagnostic{fileName_, {}, "не удалось открыть файл вывода asm: " + asmPath});
        return false;
    }
    out << finalAsm;
    return true;
}

void Generator::collectSymbols(AST::Module& module) {
    for (auto& decl : module.decls) collectSymbolsInDecl(*decl);
}

void Generator::collectSymbolsInDecl(AST::Decl& decl) {
    if (auto* ns = dynamic_cast<AST::NamespaceDecl*>(&decl)) {
        namespaceStack_.push_back(ns->name);
        for (auto& child : ns->decls) collectSymbolsInDecl(*child);
        namespaceStack_.pop_back();
        return;
    }
    if (auto* alias = dynamic_cast<AST::TypeAliasDecl*>(&decl)) return collectAlias(*alias);
    if (auto* st = dynamic_cast<AST::StructDecl*>(&decl)) return collectStructLayout(*st);
    if (auto* fn = dynamic_cast<AST::FunctionDecl*>(&decl)) return collectFunctionLabel(*fn);
}

void Generator::collectFunctionLabel(const AST::FunctionDecl& fn) {
    const std::string q = currentPrefix().empty() ? fn.name : currentPrefix() + "::" + fn.name;
    functionFullNames_.push_back(q);
}

void Generator::collectAlias(const AST::TypeAliasDecl& alias) {
    const std::string q = currentPrefix().empty() ? alias.name : currentPrefix() + "::" + alias.name;
    const std::string target = typeExprToString(*alias.aliasedType);
    aliases_[q] = target;
    aliases_[alias.name] = target;
}

void Generator::collectStructLayout(const AST::StructDecl& st) {
    StructLayout layout;
    layout.name = currentPrefix().empty() ? st.name : currentPrefix() + "::" + st.name;
    int offset = 0;
    int maxAlign = 1;
    for (const auto& field : st.fields) {
        FieldLayout fl;
        fl.name = field.name;
        fl.type = typeExprToString(*field.type);
        fl.size = std::max(1, typeSize(fl.type));
        fl.align = std::max(1, typeAlign(fl.type));
        offset = alignTo(offset, fl.align);
        fl.offset = offset;
        offset += fl.size;
        maxAlign = std::max(maxAlign, fl.align);
        layout.indexByName[fl.name] = layout.fields.size();
        layout.fields.push_back(std::move(fl));
    }
    layout.align = maxAlign;
    layout.size = alignTo(offset, maxAlign);
    structs_[layout.name] = layout;
    if (currentPrefix().empty()) structs_[st.name] = layout;
}

void Generator::emitModule(AST::Module& module) {
    for (auto& decl : module.decls) emitDecl(*decl);
}

void Generator::emitDecl(AST::Decl& decl) {
    if (auto* ns = dynamic_cast<AST::NamespaceDecl*>(&decl)) return emitNamespace(*ns);
    if (auto* fn = dynamic_cast<AST::FunctionDecl*>(&decl)) return emitFunction(*fn);
}

void Generator::emitNamespace(AST::NamespaceDecl& decl) {
    namespaceStack_.push_back(decl.name);
    for (auto& child : decl.decls) emitDecl(*child);
    namespaceStack_.pop_back();
}

std::string Generator::resolveNamedType(const std::vector<std::string>& path) const {
    std::string name = joinPath(path);
    if (aliases_.contains(name)) return aliases_.at(name);
    if (structs_.contains(name)) return structs_.at(name).name;
    if (path.size() == 1 && !namespaceStack_.empty()) {
        const std::string q = currentPrefix() + "::" + path[0];
        if (aliases_.contains(q)) return aliases_.at(q);
        if (structs_.contains(q)) return structs_.at(q).name;
    }
    return name;
}

std::string Generator::typeExprToString(const AST::TypeExpr& typeExpr) const {
    if (const auto* named = dynamic_cast<const AST::NamedType*>(&typeExpr)) return resolveNamedType(named->path);
    if (const auto* arr = dynamic_cast<const AST::ArrayType*>(&typeExpr)) {
        return "[" + typeExprToString(*arr->elementType) + "; " + std::to_string(arr->size) + "]";
    }
    return "<error>";
}

std::string Generator::exprType(const AST::Expr& expr) const {
    return expr.semanticType.value_or("");
}

bool Generator::isFloatType(const std::string& type) const { return type == "float32" || type == "float64"; }
bool Generator::isSignedIntType(const std::string& type) const { return startsWith(type, "int") && (type == "int8" || type == "int16" || type == "int32" || type == "int64"); }
bool Generator::isUnsignedIntType(const std::string& type) const { return startsWith(type, "uint") && (type == "uint8" || type == "uint16" || type == "uint32" || type == "uint64"); }
bool Generator::isIntegerType(const std::string& type) const { return isSignedIntType(type) || isUnsignedIntType(type); }
bool Generator::isBoolType(const std::string& type) const { return type == "bool"; }
bool Generator::isStringType(const std::string& type) const { return type == "string"; }
bool Generator::isScalarType(const std::string& type) const { return isIntegerType(type) || isFloatType(type) || isBoolType(type) || isStringType(type) || type == "unit" || type.empty(); }
bool Generator::isAggregateType(const std::string& type) const { return parseArrayType(type).valid || findStruct(type); }

Generator::ArrayInfo Generator::parseArrayType(const std::string& rawType) const {
    const std::string type = trim(rawType);
    if (type.size() < 5 || type.front() != '[' || type.back() != ']') return {};
    int depth = 0;
    for (std::size_t i = 1; i + 1 < type.size(); ++i) {
        const char c = type[i];
        if (c == '[') ++depth;
        else if (c == ']') --depth;
        else if (c == ';' && depth == 0) {
            const std::string elem = trim(type.substr(1, i - 1));
            const std::string n = trim(type.substr(i + 1, type.size() - i - 2));
            try {
                return ArrayInfo{elem, static_cast<std::uint64_t>(std::stoull(n)), true};
            } catch (...) {
                return {};
            }
        }
    }
    return {};
}

const Generator::StructLayout* Generator::findStruct(const std::string& type) const {
    auto it = structs_.find(type);
    if (it != structs_.end()) return &it->second;
    return nullptr;
}

const Generator::FieldLayout* Generator::findField(const std::string& type, const std::string& field) const {
    const StructLayout* st = findStruct(type);
    if (!st) return nullptr;
    auto it = st->indexByName.find(field);
    if (it == st->indexByName.end()) return nullptr;
    return &st->fields[it->second];
}

int Generator::typeSize(const std::string& type) const {
    if (type == "unit" || type.empty()) return 0;
    if (type == "bool" || type == "int8" || type == "uint8") return 1;
    if (type == "int16" || type == "uint16") return 2;
    if (type == "int32" || type == "uint32" || type == "float32") return 4;
    if (type == "int64" || type == "uint64" || type == "float64" || type == "string") return 8;
    if (auto arr = parseArrayType(type); arr.valid) return static_cast<int>(typeSize(arr.elementType) * arr.size);
    if (const auto* st = findStruct(type)) return st->size;
    return 8;
}

int Generator::typeAlign(const std::string& type) const {
    if (type == "unit" || type.empty()) return 1;
    if (auto arr = parseArrayType(type); arr.valid) return typeAlign(arr.elementType);
    if (const auto* st = findStruct(type)) return st->align;
    return std::min(8, std::max(1, typeSize(type)));
}

int Generator::computeFrameSize(AST::FunctionDecl& fn) {
    FunctionContext ctx(*this);
    ctx.pushScope();
    for (auto& param : fn.params) ctx.addLocal(param.name, typeExprToString(*param.type));
    scanBlockForFrame(*fn.body, ctx);
    return alignTo(ctx.nextOffset, 16);
}

void Generator::scanBlockForFrame(AST::BlockStmt& block, FunctionContext& ctx) {
    ctx.pushScope();
    for (auto& stmt : block.statements) scanStmtForFrame(*stmt, ctx);
    ctx.popScope();
}

void Generator::scanStmtForFrame(AST::Stmt& stmt, FunctionContext& ctx) {
    if (auto* let = dynamic_cast<AST::LetStmt*>(&stmt)) {
        const std::string type = let->explicitType ? typeExprToString(*let->explicitType) : exprType(*let->initializer);
        ctx.addLocal(let->name, type);
        return;
    }
    if (auto* var = dynamic_cast<AST::VarStmt*>(&stmt)) {
        ctx.addLocal(var->name, typeExprToString(*var->explicitType));
        return;
    }
    if (auto* block = dynamic_cast<AST::BlockStmt*>(&stmt)) return scanBlockForFrame(*block, ctx);
    if (auto* ifs = dynamic_cast<AST::IfStmt*>(&stmt)) {
        scanBlockForFrame(*ifs->thenBlock, ctx);
        if (ifs->elseBranch) scanStmtForFrame(*ifs->elseBranch, ctx);
        return;
    }
    if (auto* wh = dynamic_cast<AST::WhileStmt*>(&stmt)) return scanBlockForFrame(*wh->body, ctx);
}

void Generator::emitFunction(AST::FunctionDecl& fn) {
    const std::string qualified = currentPrefix().empty() ? fn.name : currentPrefix() + "::" + fn.name;
    const std::string label = asmSymbolForQualifiedName(qualified);
    const int frameSize = computeFrameSize(fn);

    FunctionContext ctx(*this);
    ctx.returnLabel = freshLabel("return_");
    ctx.pushScope();

    emit(label + ":");
    emit("    push rbp");
    emit("    mov rbp, rsp");
    if (frameSize > 0) emit("    sub rsp, " + std::to_string(frameSize));

    static const char* intArgRegs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
    static const char* xmmArgRegs[] = {"xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"};
    std::size_t intArg = 0;
    std::size_t xmmArg = 0;
    for (auto& param : fn.params) {
        const std::string type = typeExprToString(*param.type);
        Local local = ctx.addLocal(param.name, type);
        if (isFloatType(type)) {
            if (xmmArg >= 8) addDiagnostic(fn, "codegen пока поддерживает не более 8 float-параметров");
            else if (type == "float32") emit(std::string("    movss ") + localAddress(local) + ", " + xmmArgRegs[xmmArg++]);
            else emit(std::string("    movsd ") + localAddress(local) + ", " + xmmArgRegs[xmmArg++]);
        } else if (isAggregateType(type)) {
            if (intArg >= 6) addDiagnostic(fn, "codegen пока поддерживает не более 6 integer/aggregate-параметров");
            else {
                emit(std::string("    mov rsi, ") + intArgRegs[intArg++]);
                emit("    lea rdi, " + localAddress(local));
                emitCopyBytes("rdi", "rsi", local.size);
            }
        } else {
            if (intArg >= 6) addDiagnostic(fn, "codegen пока поддерживает не более 6 integer/string-параметров");
            else {
                emit(std::string("    mov rax, ") + intArgRegs[intArg++]);
                emit("    lea rdi, " + localAddress(local));
                emitStoreToAddress("rdi", type);
            }
        }
    }

    emitBlock(*fn.body, ctx, false);
    emit(ctx.returnLabel + ":");
    emit("    mov rsp, rbp");
    emit("    pop rbp");
    emit("    ret");
    emit("");
}

void Generator::emitBlock(AST::BlockStmt& block, FunctionContext& ctx, bool createScope) {
    if (createScope) ctx.pushScope();
    for (auto& stmt : block.statements) emitStmt(*stmt, ctx);
    if (createScope) ctx.popScope();
}

void Generator::emitStmt(AST::Stmt& stmt, FunctionContext& ctx) {
    if (dynamic_cast<AST::EmptyStmt*>(&stmt)) return;
    if (auto* block = dynamic_cast<AST::BlockStmt*>(&stmt)) return emitBlock(*block, ctx);
    if (auto* let = dynamic_cast<AST::LetStmt*>(&stmt)) {
        const std::string type = let->explicitType ? typeExprToString(*let->explicitType) : exprType(*let->initializer);
        Local local = ctx.addLocal(let->name, type);
        emit("    lea rdi, " + localAddress(local));
        emitInitToAddress(*let->initializer, type, ctx);
        return;
    }
    if (auto* var = dynamic_cast<AST::VarStmt*>(&stmt)) {
        const std::string type = typeExprToString(*var->explicitType);
        Local local = ctx.addLocal(var->name, type);
        emit("    lea rdi, " + localAddress(local));
        emitInitToAddress(*var->initializer, type, ctx);
        return;
    }
    if (auto* asg = dynamic_cast<AST::AssignStmt*>(&stmt)) {
        const std::string valueType = exprType(*asg->value);
        if (isAggregateType(valueType)) {
            emitAddressOf(*asg->target, ctx);
            emitPush(ctx, "rax");
            emitAddressOf(*asg->value, ctx);
            emit("    mov rsi, rax");
            emitPop(ctx, "rdi");
            emitCopyBytes("rdi", "rsi", typeSize(valueType));
        } else {
            emitExpr(*asg->value, ctx);
            emitStoreToLValue(*asg->target, valueType, ctx);
        }
        return;
    }
    if (auto* exprStmt = dynamic_cast<AST::ExprStmt*>(&stmt)) return emitExpr(*exprStmt->expr, ctx);
    if (auto* ifs = dynamic_cast<AST::IfStmt*>(&stmt)) return emitIf(*ifs, ctx);
    if (auto* wh = dynamic_cast<AST::WhileStmt*>(&stmt)) return emitWhile(*wh, ctx);
    if (auto* br = dynamic_cast<AST::BreakStmt*>(&stmt)) {
        if (ctx.breakLabels.empty()) addDiagnostic(*br, "break вне цикла на стадии codegen");
        else emit("    jmp " + ctx.breakLabels.back());
        return;
    }
    if (auto* cont = dynamic_cast<AST::ContinueStmt*>(&stmt)) {
        if (ctx.continueLabels.empty()) addDiagnostic(*cont, "continue вне цикла на стадии codegen");
        else emit("    jmp " + ctx.continueLabels.back());
        return;
    }
    if (auto* ret = dynamic_cast<AST::ReturnStmt*>(&stmt)) {
        if (ret->value) emitExpr(*ret->value, ctx);
        emit("    jmp " + ctx.returnLabel);
        return;
    }
    addDiagnostic(stmt, "неизвестная инструкция для codegen");
}

void Generator::emitIf(AST::IfStmt& stmt, FunctionContext& ctx) {
    const std::string elseLabel = freshLabel("else_");
    const std::string endLabel = freshLabel("endif_");
    emitExpr(*stmt.condition, ctx);
    emit("    cmp rax, 0");
    emit("    je " + elseLabel);
    emitBlock(*stmt.thenBlock, ctx);
    emit("    jmp " + endLabel);
    emit(elseLabel + ":");
    if (stmt.elseBranch) emitStmt(*stmt.elseBranch, ctx);
    emit(endLabel + ":");
}

void Generator::emitWhile(AST::WhileStmt& stmt, FunctionContext& ctx) {
    const std::string condLabel = freshLabel("while_cond_");
    const std::string endLabel = freshLabel("while_end_");
    ctx.continueLabels.push_back(condLabel);
    ctx.breakLabels.push_back(endLabel);
    emit(condLabel + ":");
    emitExpr(*stmt.condition, ctx);
    emit("    cmp rax, 0");
    emit("    je " + endLabel);
    emitBlock(*stmt.body, ctx);
    emit("    jmp " + condLabel);
    emit(endLabel + ":");
    ctx.breakLabels.pop_back();
    ctx.continueLabels.pop_back();
}

void Generator::emitExpr(AST::Expr& expr, FunctionContext& ctx) {
    const std::string type = exprType(expr);
    if (auto* i = dynamic_cast<AST::IntLiteralExpr*>(&expr)) {
        emit("    mov rax, " + i->lexeme);
        emitNormalizeInteger(type.empty() ? "int32" : type);
        return;
    }
    if (auto* f = dynamic_cast<AST::FloatLiteralExpr*>(&expr)) {
        const std::string t = type.empty() ? "float64" : type;
        const std::string label = floatLabel(f->lexeme, t);
        if (t == "float32") emit("    movss xmm0, [rel " + label + "]");
        else emit("    movsd xmm0, [rel " + label + "]");
        return;
    }
    if (auto* b = dynamic_cast<AST::BoolLiteralExpr*>(&expr)) {
        emit(std::string("    mov rax, ") + (b->value ? "1" : "0"));
        return;
    }
    if (auto* s = dynamic_cast<AST::StringLiteralExpr*>(&expr)) {
        emit("    lea rax, [rel " + stringLabel(s->value) + "]");
        return;
    }
    if (auto* name = dynamic_cast<AST::NameExpr*>(&expr)) {
        if (name->path.size() == 1) {
            if (const Local* local = ctx.findLocal(name->path[0])) {
                if (isAggregateType(local->type)) emit("    lea rax, " + localAddress(*local));
                else {
                    emit("    lea rax, " + localAddress(*local));
                    emitLoadFromAddress("rax", local->type);
                }
                return;
            }
        }
        addDiagnostic(expr, "codegen не может использовать имя как значение: " + joinPath(name->path));
        emit("    xor rax, rax");
        return;
    }
    if (auto* unary = dynamic_cast<AST::UnaryExpr*>(&expr)) {
        emitExpr(*unary->operand, ctx);
        const std::string operandType = exprType(*unary->operand);
        if (unary->op == "-") {
            if (isFloatType(operandType)) {
                const std::string label = floatLabel("-1.0", operandType);
                if (operandType == "float32") emit("    mulss xmm0, [rel " + label + "]");
                else emit("    mulsd xmm0, [rel " + label + "]");
            } else {
                emit("    neg rax");
                emitNormalizeInteger(operandType);
            }
        } else if (unary->op == "!") {
            emit("    cmp rax, 0");
            emit("    sete al");
            emit("    movzx rax, al");
        }
        return;
    }
    if (auto* bin = dynamic_cast<AST::BinaryExpr*>(&expr)) return emitBinary(*bin, ctx);
    if (auto* cast = dynamic_cast<AST::CastExpr*>(&expr)) return emitCast(*cast, ctx);
    if (auto* call = dynamic_cast<AST::CallExpr*>(&expr)) return emitCall(*call, ctx);
    if (dynamic_cast<AST::ArrayLiteralExpr*>(&expr) || dynamic_cast<AST::StructLiteralExpr*>(&expr)) {
        // Материализуем агрегатный литерал во временную память на стеке.
        // После этого rax указывает на временный объект.
        const int sz = std::max(8, alignTo(typeSize(type), 8));
        emit("    sub rsp, " + std::to_string(sz));
        emit("    mov rdi, rsp");
        emitInitToAddress(expr, type, ctx);
        emit("    mov rax, rsp");
        // Примечание: после использования стек не восстанавливается здесь —
        // вызывающий код (return, call) сам управляет стеком через emitInitToAddress.
        // Для безопасности сразу восстанавливаем, а адрес уже в rax.
        emit("    add rsp, " + std::to_string(sz));
        return;
    }
    if (auto* field = dynamic_cast<AST::FieldExpr*>(&expr)) {
        emitAddressOf(*field, ctx);
        if (isAggregateType(type)) return;
        emitLoadFromAddress("rax", type);
        return;
    }
    if (auto* idx = dynamic_cast<AST::IndexExpr*>(&expr)) {
        emitAddressOf(*idx, ctx);
        if (isAggregateType(type)) return;
        emitLoadFromAddress("rax", type);
        return;
    }
    addDiagnostic(expr, "неизвестное выражение для codegen");
    emit("    xor rax, rax");
}

void Generator::emitBinary(AST::BinaryExpr& expr, FunctionContext& ctx) {
    if (expr.op == "&&") return emitLogicalAnd(expr, ctx);
    if (expr.op == "||") return emitLogicalOr(expr, ctx);
    const std::string type = exprType(expr);

    if (isFloatType(exprType(*expr.left)) || isFloatType(exprType(*expr.right))) {
        return emitFloatBinary(expr, ctx, exprType(*expr.left));
    }

    if (type == "string") {
        if (expr.op == "+") {
            emitExpr(*expr.left, ctx);
            emitPush(ctx, "rax");
            emitExpr(*expr.right, ctx);
            emit("    mov rsi, rax");
            emitPop(ctx, "rdi");
            emitCallInstruction(ctx, "astra_string_concat");
            return;
        }
        if (expr.op == "==" || expr.op == "!=") {
            emitExpr(*expr.left, ctx);
            emitPush(ctx, "rax");
            emitExpr(*expr.right, ctx);
            emit("    mov rsi, rax");
            emitPop(ctx, "rdi");
            emitCallInstruction(ctx, "astra_string_eq");
            if (expr.op == "!=") emit("    xor rax, 1");
            return;
        }
    }

    if (isAggregateType(exprType(*expr.left)) && (expr.op == "==" || expr.op == "!=")) {
        // Покомпонентное сравнение агрегатов.
        // Алгоритм: сохраняем адреса обоих операндов, затем сравниваем
        // элементы/поля по одному; при первом несовпадении выходим с 0.
        const std::string leftType = exprType(*expr.left);
        const bool isEq = (expr.op == "==");
        const std::string trueLabel  = freshLabel("agg_eq_true_");
        const std::string falseLabel = freshLabel("agg_eq_false_");
        const std::string endLabel   = freshLabel("agg_eq_end_");

        // Получаем адрес левого операнда → rsi
        emitAddressOf(*expr.left, ctx);
        emitPush(ctx, "rax");  // сохраняем адрес левого

        // Получаем адрес правого операнда → rdx
        emitAddressOf(*expr.right, ctx);
        emit("    mov rdx, rax");

        emitPop(ctx, "rsi");  // адрес левого

        // Генерируем побайтовое сравнение для массива или по полям для структуры.
        // Используем подход: сравниваем последовательно каждый элемент/поле.
        // rsi = адрес левого, rdx = адрес правого
        const ArrayInfo arrInfo = parseArrayType(leftType);
        if (arrInfo.valid) {
            const int elemSize = std::max(1, typeSize(arrInfo.elementType));
            for (std::uint64_t i = 0; i < arrInfo.size; ++i) {
                const int off = static_cast<int>(i) * elemSize;
                if (isFloatType(arrInfo.elementType)) {
                    if (arrInfo.elementType == "float32") {
                        emit("    movss xmm0, dword [rsi + " + std::to_string(off) + "]");
                        emit("    ucomiss xmm0, dword [rdx + " + std::to_string(off) + "]");
                    } else {
                        emit("    movsd xmm0, qword [rsi + " + std::to_string(off) + "]");
                        emit("    ucomisd xmm0, qword [rdx + " + std::to_string(off) + "]");
                    }
                    emit("    jne " + falseLabel);
                } else {
                    // Сравниваем целочисленный элемент нужного размера
                    if (elemSize == 1) {
                        emit("    mov al, byte [rsi + " + std::to_string(off) + "]");
                        emit("    cmp al, byte [rdx + " + std::to_string(off) + "]");
                    } else if (elemSize == 2) {
                        emit("    mov ax, word [rsi + " + std::to_string(off) + "]");
                        emit("    cmp ax, word [rdx + " + std::to_string(off) + "]");
                    } else if (elemSize == 4) {
                        emit("    mov eax, dword [rsi + " + std::to_string(off) + "]");
                        emit("    cmp eax, dword [rdx + " + std::to_string(off) + "]");
                    } else {
                        emit("    mov rax, qword [rsi + " + std::to_string(off) + "]");
                        emit("    cmp rax, qword [rdx + " + std::to_string(off) + "]");
                    }
                    emit("    jne " + falseLabel);
                }
            }
        } else if (const StructLayout* st = findStruct(leftType)) {
            for (const auto& fl : st->fields) {
                if (isFloatType(fl.type)) {
                    if (fl.type == "float32") {
                        emit("    movss xmm0, dword [rsi + " + std::to_string(fl.offset) + "]");
                        emit("    ucomiss xmm0, dword [rdx + " + std::to_string(fl.offset) + "]");
                    } else {
                        emit("    movsd xmm0, qword [rsi + " + std::to_string(fl.offset) + "]");
                        emit("    ucomisd xmm0, qword [rdx + " + std::to_string(fl.offset) + "]");
                    }
                    emit("    jne " + falseLabel);
                } else if (isAggregateType(fl.type)) {
                    // Вложенный агрегат: сравниваем побайтово через rep cmpsb
                    const int sz = std::max(1, typeSize(fl.type));
                    emit("    lea rdi, [rsi + " + std::to_string(fl.offset) + "]");
                    emit("    lea rcx, [rdx + " + std::to_string(fl.offset) + "]");
                    // Побайтовое сравнение sz байт
                    for (int b = 0; b < sz; b += 8) {
                        const int chunk = std::min(8, sz - b);
                        if (chunk == 8) {
                            emit("    mov rax, qword [rdi + " + std::to_string(b) + "]");
                            emit("    cmp rax, qword [rcx + " + std::to_string(b) + "]");
                        } else if (chunk >= 4) {
                            emit("    mov eax, dword [rdi + " + std::to_string(b) + "]");
                            emit("    cmp eax, dword [rcx + " + std::to_string(b) + "]");
                        } else {
                            emit("    mov al, byte [rdi + " + std::to_string(b) + "]");
                            emit("    cmp al, byte [rcx + " + std::to_string(b) + "]");
                        }
                        emit("    jne " + falseLabel);
                    }
                } else {
                    const int off = fl.offset;
                    if (fl.size == 1) {
                        emit("    mov al, byte [rsi + " + std::to_string(off) + "]");
                        emit("    cmp al, byte [rdx + " + std::to_string(off) + "]");
                    } else if (fl.size == 2) {
                        emit("    mov ax, word [rsi + " + std::to_string(off) + "]");
                        emit("    cmp ax, word [rdx + " + std::to_string(off) + "]");
                    } else if (fl.size == 4) {
                        emit("    mov eax, dword [rsi + " + std::to_string(off) + "]");
                        emit("    cmp eax, dword [rdx + " + std::to_string(off) + "]");
                    } else {
                        emit("    mov rax, qword [rsi + " + std::to_string(off) + "]");
                        emit("    cmp rax, qword [rdx + " + std::to_string(off) + "]");
                    }
                    emit("    jne " + falseLabel);
                }
            }
        } else {
            // Неизвестный агрегатный тип: побайтово по размеру
            const int sz = std::max(1, typeSize(leftType));
            for (int b = 0; b < sz; ++b) {
                emit("    mov al, byte [rsi + " + std::to_string(b) + "]");
                emit("    cmp al, byte [rdx + " + std::to_string(b) + "]");
                emit("    jne " + falseLabel);
            }
        }

        // Все сравнения прошли — равны
        emit(trueLabel + ":");
        emit(isEq ? "    mov rax, 1" : "    xor rax, rax");
        emit("    jmp " + endLabel);
        emit(falseLabel + ":");
        emit(isEq ? "    xor rax, rax" : "    mov rax, 1");
        emit(endLabel + ":");
        return;
    }

    emitExpr(*expr.left, ctx);
    emitPush(ctx, "rax");
    emitExpr(*expr.right, ctx);
    emit("    mov rbx, rax");
    emitPop(ctx, "rax");

    const auto& op = expr.op;
    const std::string operandType = exprType(*expr.left);
    if (op == "+") emit("    add rax, rbx");
    else if (op == "-") emit("    sub rax, rbx");
    else if (op == "*") emit("    imul rax, rbx");
    else if (op == "/" || op == "%") {
        const std::string okLabel = freshLabel("div_ok_");
        emit("    cmp rbx, 0");
        emit("    jne " + okLabel);
        emit("    mov rdi, " + std::to_string(expr.span.begin.line));
        emitCallInstruction(ctx, "astra_rt_div_zero");
        emit(okLabel + ":");
        if (isUnsignedIntType(operandType)) {
            emit("    xor rdx, rdx");
            emit("    div rbx");
        } else {
            emit("    cqo");
            emit("    idiv rbx");
        }
        if (op == "%") emit("    mov rax, rdx");
    } else if (op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=") {
        emit("    cmp rax, rbx");
        if (op == "==") emit("    sete al");
        else if (op == "!=") emit("    setne al");
        else if (isUnsignedIntType(operandType)) {
            if (op == "<") emit("    setb al");
            else if (op == "<=") emit("    setbe al");
            else if (op == ">") emit("    seta al");
            else emit("    setae al");
        } else {
            if (op == "<") emit("    setl al");
            else if (op == "<=") emit("    setle al");
            else if (op == ">") emit("    setg al");
            else emit("    setge al");
        }
        emit("    movzx rax, al");
        return;
    } else {
        addDiagnostic(expr, "неизвестный бинарный оператор в codegen: " + op);
    }
    emitNormalizeInteger(type);
}

void Generator::emitFloatBinary(AST::BinaryExpr& expr, FunctionContext& ctx, const std::string& type) {
    const bool f32 = type == "float32";
    emitExpr(*expr.left, ctx);
    emit("    sub rsp, 8");
    ctx.stackBytes += 8;
    if (f32) emit("    movss [rsp], xmm0");
    else emit("    movsd [rsp], xmm0");
    emitExpr(*expr.right, ctx);
    if (f32) emit("    movss xmm1, [rsp]");
    else emit("    movsd xmm1, [rsp]");
    emit("    add rsp, 8");
    ctx.stackBytes -= 8;

    const std::string suffix = f32 ? "ss" : "sd";
    const auto& op = expr.op;
    if (op == "+") emit("    add" + suffix + " xmm1, xmm0");
    else if (op == "-") emit("    sub" + suffix + " xmm1, xmm0");
    else if (op == "*") emit("    mul" + suffix + " xmm1, xmm0");
    else if (op == "/") emit("    div" + suffix + " xmm1, xmm0");
    else if (op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=") {
        emit("    ucomi" + suffix + " xmm1, xmm0");
        if (op == "==") emit("    sete al");
        else if (op == "!=") emit("    setne al");
        else if (op == "<") emit("    setb al");
        else if (op == "<=") emit("    setbe al");
        else if (op == ">") emit("    seta al");
        else emit("    setae al");
        emit("    movzx rax, al");
        return;
    } else {
        addDiagnostic(expr, "оператор " + op + " не поддержан для float в codegen");
        return;
    }
    if (f32) emit("    movaps xmm0, xmm1");
    else emit("    movapd xmm0, xmm1");
}

void Generator::emitLogicalAnd(AST::BinaryExpr& expr, FunctionContext& ctx) {
    const std::string falseLabel = freshLabel("and_false_");
    const std::string endLabel = freshLabel("and_end_");
    emitExpr(*expr.left, ctx);
    emit("    cmp rax, 0");
    emit("    je " + falseLabel);
    emitExpr(*expr.right, ctx);
    emit("    cmp rax, 0");
    emit("    je " + falseLabel);
    emit("    mov rax, 1");
    emit("    jmp " + endLabel);
    emit(falseLabel + ":");
    emit("    xor rax, rax");
    emit(endLabel + ":");
}

void Generator::emitLogicalOr(AST::BinaryExpr& expr, FunctionContext& ctx) {
    const std::string trueLabel = freshLabel("or_true_");
    const std::string endLabel = freshLabel("or_end_");
    emitExpr(*expr.left, ctx);
    emit("    cmp rax, 0");
    emit("    jne " + trueLabel);
    emitExpr(*expr.right, ctx);
    emit("    cmp rax, 0");
    emit("    jne " + trueLabel);
    emit("    xor rax, rax");
    emit("    jmp " + endLabel);
    emit(trueLabel + ":");
    emit("    mov rax, 1");
    emit(endLabel + ":");
}

void Generator::emitCast(AST::CastExpr& expr, FunctionContext& ctx) {
    const std::string from = exprType(*expr.value);
    const std::string to = exprType(expr);
    emitExpr(*expr.value, ctx);
    if (from == to) return;

    if (isIntegerType(from) && isIntegerType(to)) {
        emitNormalizeInteger(to);
        return;
    }
    if (isIntegerType(from) && isFloatType(to)) {
        if (to == "float32") emit("    cvtsi2ss xmm0, rax");
        else emit("    cvtsi2sd xmm0, rax");
        return;
    }
    if (isFloatType(from) && isIntegerType(to)) {
        if (from == "float32") emit("    cvttss2si rax, xmm0");
        else emit("    cvttsd2si rax, xmm0");
        emitNormalizeInteger(to);
        return;
    }
    if (from == "float32" && to == "float64") {
        emit("    cvtss2sd xmm0, xmm0");
        return;
    }
    if (from == "float64" && to == "float32") {
        emit("    cvtsd2ss xmm0, xmm0");
        return;
    }
    addDiagnostic(expr, "недопустимое приведение на стадии codegen из " + from + " в " + to);
}

void Generator::emitCall(AST::CallExpr& expr, FunctionContext& ctx) {
    auto* calleeName = dynamic_cast<AST::NameExpr*>(expr.callee.get());
    if (!calleeName) {
        addDiagnostic(expr, "codegen поддерживает вызов только по имени функции");
        emit("    xor rax, rax");
        return;
    }

    const std::string name = joinPath(calleeName->path);
    if (calleeName->path.size() == 1 && name == "print") {
        if (expr.args.size() != 1) {
            addDiagnostic(expr, "print ожидает один аргумент");
            return;
        }
        const std::string t = exprType(*expr.args[0]);
        emitExpr(*expr.args[0], ctx);
        if (t == "float32") emitCallInstruction(ctx, "astra_print_f32");
        else if (t == "float64") emitCallInstruction(ctx, "astra_print_f64");
        else {
            emit("    mov rdi, rax");
            if (t == "string") emitCallInstruction(ctx, "astra_print_string");
            else if (t == "bool") emitCallInstruction(ctx, "astra_print_bool");
            else if (t == "uint32") emitCallInstruction(ctx, "astra_print_u32");
            else if (t == "uint64") emitCallInstruction(ctx, "astra_print_u64");
            else if (t == "int64") emitCallInstruction(ctx, "astra_print_i64");
            else emitCallInstruction(ctx, "astra_print_i32");
        }
        emit("    xor rax, rax");
        return;
    }
    if (calleeName->path.size() == 1 && name == "input") {
        emitCallInstruction(ctx, "astra_input_string");
        return;
    }
    if (calleeName->path.size() == 1 && name == "len") {
        if (expr.args.size() == 1) {
            emitExpr(*expr.args[0], ctx);
            emit("    mov rdi, rax");
            emitCallInstruction(ctx, "astra_string_len");
        }
        return;
    }
    if (calleeName->path.size() == 1 && name == "exit") {
        if (expr.args.size() == 1) {
            emitExpr(*expr.args[0], ctx);
            emit("    mov rdi, rax");
            emitCallInstruction(ctx, "astra_exit");
        }
        return;
    }
    if (calleeName->path.size() == 1 && name == "panic") {
        if (expr.args.size() == 1) {
            emitExpr(*expr.args[0], ctx);
            emit("    mov rdi, rax");
            emitCallInstruction(ctx, "astra_panic");
        }
        return;
    }

    static const char* intArgRegs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
    static const char* xmmArgRegs[] = {"xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"};
    std::size_t intCount = 0;
    std::size_t xmmCount = 0;
    int aggregateTempBytes = 0;

    struct SavedArg { bool isFloat = false; std::string type; std::size_t regIndex = 0; };
    std::vector<SavedArg> saved;
    for (auto& arg : expr.args) {
        const std::string t = exprType(*arg);
        if (isFloatType(t)) {
            if (xmmCount >= 8) addDiagnostic(expr, "слишком много float-аргументов для codegen");
            emitExpr(*arg, ctx);
            emit("    sub rsp, 8");
            ctx.stackBytes += 8;
            if (t == "float32") emit("    movss [rsp], xmm0"); else emit("    movsd [rsp], xmm0");
            saved.push_back({true, t, xmmCount++});
        } else {
            if (intCount >= 6) addDiagnostic(expr, "слишком много integer/aggregate-аргументов для codegen");
            if (isAggregateType(t)) {
                if (dynamic_cast<AST::ArrayLiteralExpr*>(arg.get()) || dynamic_cast<AST::StructLiteralExpr*>(arg.get())) {
                    const int tempSize = alignTo(std::max(1, typeSize(t)), 8);
                    emit("    sub rsp, " + std::to_string(tempSize));
                    ctx.stackBytes += tempSize;
                    aggregateTempBytes += tempSize;
                    emit("    mov rdi, rsp");
                    emitInitToAddress(*arg, t, ctx);
                    emit("    mov rax, rsp");
                } else {
                    emitAddressOf(*arg, ctx);
                }
            } else emitExpr(*arg, ctx);
            emitPush(ctx, "rax");
            saved.push_back({false, t, intCount++});
        }
    }
    for (std::size_t i = saved.size(); i > 0; --i) {
        const auto& arg = saved[i - 1];
        if (arg.isFloat) {
            if (arg.type == "float32") emit(std::string("    movss ") + xmmArgRegs[arg.regIndex] + ", [rsp]");
            else emit(std::string("    movsd ") + xmmArgRegs[arg.regIndex] + ", [rsp]");
            emit("    add rsp, 8");
            ctx.stackBytes -= 8;
        } else {
            emitPop(ctx, intArgRegs[arg.regIndex]);
        }
    }
    emitCallInstruction(ctx, asmSymbolForPath(calleeName->path));
    if (aggregateTempBytes > 0) {
        emit("    add rsp, " + std::to_string(aggregateTempBytes));
        ctx.stackBytes -= aggregateTempBytes;
    }
}

void Generator::emitAddressOf(AST::Expr& expr, FunctionContext& ctx) {
    if (auto* name = dynamic_cast<AST::NameExpr*>(&expr)) {
        if (name->path.size() == 1) {
            if (const Local* local = ctx.findLocal(name->path[0])) {
                emit("    lea rax, " + localAddress(*local));
                return;
            }
        }
        addDiagnostic(expr, "невозможно получить адрес имени: " + joinPath(name->path));
        emit("    xor rax, rax");
        return;
    }
    if (auto* field = dynamic_cast<AST::FieldExpr*>(&expr)) {
        const std::string objectType = exprType(*field->object);
        const FieldLayout* fl = findField(objectType, field->field);
        if (!fl) {
            addDiagnostic(expr, "не найден layout поля " + field->field + " у типа " + objectType);
            emit("    xor rax, rax");
            return;
        }
        emitAddressOf(*field->object, ctx);
        if (fl->offset != 0) emit("    add rax, " + std::to_string(fl->offset));
        return;
    }
    if (auto* idx = dynamic_cast<AST::IndexExpr*>(&expr)) {
        const std::string objectType = exprType(*idx->object);
        const ArrayInfo arr = parseArrayType(objectType);
        if (!arr.valid) {
            addDiagnostic(expr, "не найден layout массива для типа " + objectType);
            emit("    xor rax, rax");
            return;
        }
        const int elemSize = std::max(1, typeSize(arr.elementType));
        emitAddressOf(*idx->object, ctx);
        emitPush(ctx, "rax");
        emitExpr(*idx->index, ctx);
        const std::string okLabel = freshLabel("idx_ok_");
        emit("    cmp rax, 0");
        emit("    jl " + okLabel + "_bad");
        emit("    cmp rax, " + std::to_string(arr.size));
        emit("    jge " + okLabel + "_bad");
        emit("    jmp " + okLabel);
        emit(okLabel + "_bad:");
        emit("    mov rdi, " + std::to_string(idx->span.begin.line));
        emitCallInstruction(ctx, "astra_rt_oob");
        emit(okLabel + ":");
        if (elemSize != 1) emit("    imul rax, " + std::to_string(elemSize));
        emitPop(ctx, "rbx");
        emit("    add rax, rbx");
        return;
    }
    // Для агрегатных выражений: материализуем во временный буфер стека
    const std::string exprT = exprType(expr);
    if (isAggregateType(exprT)) {
        const int sz = std::max(8, alignTo(typeSize(exprT), 8));
        emit("    sub rsp, " + std::to_string(sz));
        ctx.stackBytes += sz;
        emit("    mov rdi, rsp");
        emitInitToAddress(expr, exprT, ctx);
        emit("    mov rax, rsp");
        return;
    }
    addDiagnostic(expr, "выражение не имеет адреса для codegen");
    emit("    xor rax, rax");
}


void Generator::emitStoreToLValue(AST::Expr& target, const std::string& valueType, FunctionContext& ctx) {
    emitPush(ctx, "rax");
    emitAddressOf(target, ctx);
    emit("    mov rdi, rax");
    emitPop(ctx, "rax");
    emitStoreToAddress("rdi", valueType);
}

void Generator::emitInitToAddress(AST::Expr& expr, const std::string& targetType, FunctionContext& ctx) {
    if (auto* arrLit = dynamic_cast<AST::ArrayLiteralExpr*>(&expr)) {
        const ArrayInfo arr = parseArrayType(targetType);
        if (!arr.valid) {
            addDiagnostic(expr, "литерал массива используется для не-массивного типа " + targetType);
            return;
        }
        const int elemSize = std::max(1, typeSize(arr.elementType));
        for (std::size_t i = 0; i < arrLit->elements.size(); ++i) {
            emitPush(ctx, "rdi");
            emit("    add rdi, " + std::to_string(static_cast<int>(i) * elemSize));
            emitInitToAddress(*arrLit->elements[i], arr.elementType, ctx);
            emitPop(ctx, "rdi");
        }
        return;
    }
    if (auto* stLit = dynamic_cast<AST::StructLiteralExpr*>(&expr)) {
        const StructLayout* st = findStruct(targetType);
        if (!st) {
            addDiagnostic(expr, "литерал структуры используется для неизвестного типа " + targetType);
            return;
        }
        for (auto& field : stLit->fields) {
            const FieldLayout* fl = findField(targetType, field.name);
            if (!fl) continue;
            emitPush(ctx, "rdi");
            if (fl->offset != 0) emit("    add rdi, " + std::to_string(fl->offset));
            emitInitToAddress(*field.value, fl->type, ctx);
            emitPop(ctx, "rdi");
        }
        return;
    }
    if (isAggregateType(targetType)) {
        // Если это вызов функции, возвращающей агрегат — передаём скрытый указатель
        if (dynamic_cast<AST::CallExpr*>(&expr)) {
            // rdi уже содержит адрес назначения; передаём его как скрытый первый аргумент
            emitCallAggregateDest(expr, targetType, ctx);
            return;
        }
        emitPush(ctx, "rdi");
        emitAddressOf(expr, ctx);
        emit("    mov rsi, rax");
        emitPop(ctx, "rdi");
        emitCopyBytes("rdi", "rsi", typeSize(targetType));
        return;
    }
    emitPush(ctx, "rdi");
    emitExpr(expr, ctx);
    emitPop(ctx, "rdi");
    emitStoreToAddress("rdi", targetType);
}

void Generator::emitCopyBytes(const std::string& dstReg, const std::string& srcReg, int size) {
    int off = 0;
    while (size - off >= 8) {
        emit("    mov rax, " + mem(srcReg, off));
        emit("    mov " + mem(dstReg, off) + ", rax");
        off += 8;
    }
    if (size - off >= 4) {
        emit("    mov eax, dword " + mem(srcReg, off));
        emit("    mov dword " + mem(dstReg, off) + ", eax");
        off += 4;
    }
    if (size - off >= 2) {
        emit("    mov ax, word " + mem(srcReg, off));
        emit("    mov word " + mem(dstReg, off) + ", ax");
        off += 2;
    }
    if (size - off >= 1) {
        emit("    mov al, byte " + mem(srcReg, off));
        emit("    mov byte " + mem(dstReg, off) + ", al");
    }
}

void Generator::emitLoadFromAddress(const std::string& reg, const std::string& type) {
    if (type == "float32") { emit("    movss xmm0, " + mem(reg)); return; }
    if (type == "float64") { emit("    movsd xmm0, " + mem(reg)); return; }
    if (type == "bool" || type == "uint8") emit("    movzx rax, byte " + mem(reg));
    else if (type == "int8") emit("    movsx rax, byte " + mem(reg));
    else if (type == "uint16") emit("    movzx rax, word " + mem(reg));
    else if (type == "int16") emit("    movsx rax, word " + mem(reg));
    else if (type == "uint32") emit("    mov eax, dword " + mem(reg));
    else if (type == "int32") emit("    movsxd rax, dword " + mem(reg));
    else emit("    mov rax, qword " + mem(reg));
}

void Generator::emitStoreToAddress(const std::string& reg, const std::string& type) {
    if (type == "float32") { emit("    movss " + mem(reg) + ", xmm0"); return; }
    if (type == "float64") { emit("    movsd " + mem(reg) + ", xmm0"); return; }
    emitNormalizeInteger(type);
    if (type == "bool" || type == "int8" || type == "uint8") emit("    mov byte " + mem(reg) + ", al");
    else if (type == "int16" || type == "uint16") emit("    mov word " + mem(reg) + ", ax");
    else if (type == "int32" || type == "uint32") emit("    mov dword " + mem(reg) + ", eax");
    else if (type != "unit") emit("    mov qword " + mem(reg) + ", rax");
}

void Generator::emitNormalizeInteger(const std::string& type) {
    if (type == "bool" || type == "uint8") emit("    movzx rax, al");
    else if (type == "int8") emit("    movsx rax, al");
    else if (type == "uint16") emit("    movzx rax, ax");
    else if (type == "int16") emit("    movsx rax, ax");
    else if (type == "uint32") emit("    mov eax, eax");
    else if (type == "int32") emit("    movsxd rax, eax");
}

void Generator::emitCallInstruction(FunctionContext& ctx, const std::string& callee) {
    if (ctx.stackBytes % 16 != 0) {
        emit("    sub rsp, 8");
        emit("    call " + callee);
        emit("    add rsp, 8");
    } else {
        emit("    call " + callee);
    }
}

void Generator::emitPush(FunctionContext& ctx, const std::string& reg) {
    emit("    push " + reg);
    ctx.stackBytes += 8;
}

void Generator::emitPop(FunctionContext& ctx, const std::string& reg) {
    emit("    pop " + reg);
    ctx.stackBytes -= 8;
}


// Вызов функции возвращающей агрегат с передачей скрытого указателя назначения.
// По ABI: первый аргумент (rdi) = адрес буфера для результата.
// Вызывающий сохраняет rdi перед вычислением остальных аргументов.
void Generator::emitCallAggregateDest(AST::Expr& callExprBase, const std::string& /*targetType*/, FunctionContext& ctx) {
    auto* callExpr = dynamic_cast<AST::CallExpr*>(&callExprBase);
    if (!callExpr) {
        addDiagnostic(callExprBase, "emitCallAggregateDest: ожидался CallExpr");
        return;
    }
    auto* calleeName = dynamic_cast<AST::NameExpr*>(callExpr->callee.get());
    if (!calleeName) {
        addDiagnostic(*callExpr, "codegen поддерживает вызов только по имени функции");
        return;
    }

    // Сохраняем rdi (адрес назначения) на стеке
    emitPush(ctx, "rdi");

    static const char* intArgRegs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
    static const char* xmmArgRegs[] = {"xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"};
    std::size_t intCount = 0;
    std::size_t xmmCount = 0;

    struct SavedArg { bool isFloat = false; std::string type; std::size_t regIndex = 0; };
    std::vector<SavedArg> saved;

    for (auto& arg : callExpr->args) {
        const std::string t = exprType(*arg);
        if (isFloatType(t)) {
            emitExpr(*arg, ctx);
            emit("    sub rsp, 8");
            ctx.stackBytes += 8;
            if (t == "float32") emit("    movss [rsp], xmm0");
            else emit("    movsd [rsp], xmm0");
            saved.push_back({true, t, xmmCount++});
        } else {
            if (isAggregateType(t)) emitAddressOf(*arg, ctx);
            else emitExpr(*arg, ctx);
            emitPush(ctx, "rax");
            saved.push_back({false, t, intCount++});
        }
    }

    // Восстанавливаем аргументы в регистры ABI, первый целочисленный слот занят rdi (скрытый ptr)
    // rdi = адрес назначения — берём его первым
    // Поэтому остальные аргументы идут начиная с rsi
    for (std::size_t i = saved.size(); i > 0; --i) {
        const auto& arg = saved[i - 1];
        if (arg.isFloat) {
            if (arg.type == "float32") emit(std::string("    movss ") + xmmArgRegs[arg.regIndex] + ", [rsp]");
            else emit(std::string("    movsd ") + xmmArgRegs[arg.regIndex] + ", [rsp]");
            emit("    add rsp, 8");
            ctx.stackBytes -= 8;
        } else {
            // Сдвигаем на один регистр вперёд (rdi занят под скрытый ptr)
            const std::size_t realIdx = arg.regIndex + 1;
            if (realIdx < 6) emitPop(ctx, intArgRegs[realIdx]);
            else emit("    add rsp, 8"), ctx.stackBytes -= 8;  // лишний — убираем
        }
    }
    // Восстанавливаем rdi = адрес назначения
    emitPop(ctx, "rdi");
    emitCallInstruction(ctx, asmSymbolForPath(calleeName->path));
    // rax после вызова не используется; результат уже в rdi-буфере
}

std::string formatDiagnostic(const Diagnostic& diagnostic) {
    return diagnostic.file + ":" + std::to_string(diagnostic.pos.line) + ":" +
           std::to_string(diagnostic.pos.column) + ": error: " + diagnostic.message;
}

} // namespace Codegen
