#include "codegen.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace Codegen {

struct Generator::FunctionContext {
    std::vector<std::unordered_map<std::string, Local>> scopes;
    std::vector<std::string> breakLabels;
    std::vector<std::string> continueLabels;
    std::string returnLabel;
    int nextSlot = 0;

    void pushScope() { scopes.emplace_back(); }
    void popScope() { scopes.pop_back(); }

    Local addLocal(const std::string& name, std::string type) {
        Local local{-(++nextSlot * 8), std::move(type)};
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
            std::string candidate = currentPrefix() + "::" + path[0];
            if (std::find(functionFullNames_.begin(), functionFullNames_.end(), candidate) != functionFullNames_.end()) {
                qualified = candidate;
            }
        }
        return asmSymbolForQualifiedName(qualified);
    }
    return asmSymbolForQualifiedName(joinPath(path));
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
    const std::string label = ".LC" + std::to_string(stringData_.size());
    stringData_.push_back(label + ": db " + escapeNasmStringBytes(value));
    return label;
}

bool Generator::isSupportedScalarType(const std::string& type) const {
    return type == "int8" || type == "int16" || type == "int32" || type == "int64" ||
           type == "uint8" || type == "uint16" || type == "uint32" || type == "uint64" ||
           type == "bool" || type == "string" || type == "unit" || type.empty();
}

std::string Generator::exprType(const AST::Expr& expr) const {
    return expr.semanticType.value_or("");
}

bool Generator::generate(AST::Module& module, const std::string& asmPath) {
    diagnostics_.clear();
    asm_.clear();
    stringData_.clear();
    functionLabels_.clear();
    functionFullNames_.clear();
    namespaceStack_.clear();
    labelCounter_ = 0;

    collectFunctions(module);
    emitModule(module);

    if (!diagnostics_.empty()) return false;

    std::string finalAsm;
    finalAsm += "default rel\n";
    finalAsm += "section .data\n";
    for (const auto& line : stringData_) finalAsm += line + "\n";
    finalAsm += "section .text\n";
    finalAsm += "global main\n";
    finalAsm += "extern astra_print_i32\n";
    finalAsm += "extern astra_print_i64\n";
    finalAsm += "extern astra_print_bool\n";
    finalAsm += "extern astra_print_string\n";
    finalAsm += "extern astra_input_string\n";
    finalAsm += "extern astra_exit\n";
    finalAsm += "extern astra_panic\n";
    finalAsm += "extern astra_string_len\n";
    finalAsm += "extern astra_rt_div_zero\n";
    finalAsm += asm_;

    std::ofstream out(asmPath);
    if (!out) {
        diagnostics_.push_back(Diagnostic{fileName_, {}, "не удалось открыть файл вывода asm: " + asmPath});
        return false;
    }
    out << finalAsm;
    return true;
}

void Generator::collectFunctions(AST::Module& module) {
    for (auto& decl : module.decls) collectFunctionsInDecl(*decl);
}

void Generator::collectFunctionsInDecl(AST::Decl& decl) {
    if (auto* ns = dynamic_cast<AST::NamespaceDecl*>(&decl)) {
        namespaceStack_.push_back(ns->name);
        for (auto& child : ns->decls) collectFunctionsInDecl(*child);
        namespaceStack_.pop_back();
        return;
    }
    if (auto* fn = dynamic_cast<AST::FunctionDecl*>(&decl)) collectFunctionLabel(*fn);
}

void Generator::collectFunctionLabel(const AST::FunctionDecl& fn) {
    std::string q = currentPrefix().empty() ? fn.name : currentPrefix() + "::" + fn.name;
    functionFullNames_.push_back(q);
    functionLabels_.push_back(asmSymbolForQualifiedName(q));
}

void Generator::emitModule(AST::Module& module) {
    for (auto& decl : module.decls) emitDecl(*decl);
}

void Generator::emitDecl(AST::Decl& decl) {
    if (auto* ns = dynamic_cast<AST::NamespaceDecl*>(&decl)) return emitNamespace(*ns);
    if (auto* fn = dynamic_cast<AST::FunctionDecl*>(&decl)) return emitFunction(*fn);
    // Type aliases and struct declarations are compile-time-only for this backend.
}

void Generator::emitNamespace(AST::NamespaceDecl& decl) {
    namespaceStack_.push_back(decl.name);
    for (auto& child : decl.decls) emitDecl(*child);
    namespaceStack_.pop_back();
}

int Generator::countLocalSlots(AST::FunctionDecl& fn) const {
    return static_cast<int>(fn.params.size()) + countSlotsInBlock(*fn.body);
}

int Generator::countSlotsInBlock(AST::BlockStmt& block) const {
    int total = 0;
    for (auto& stmt : block.statements) total += countSlotsInStmt(*stmt);
    return total;
}

int Generator::countSlotsInStmt(AST::Stmt& stmt) const {
    if (dynamic_cast<AST::LetStmt*>(&stmt) || dynamic_cast<AST::VarStmt*>(&stmt)) return 1;
    if (auto* block = dynamic_cast<AST::BlockStmt*>(&stmt)) return countSlotsInBlock(*block);
    if (auto* ifs = dynamic_cast<AST::IfStmt*>(&stmt)) {
        int n = countSlotsInBlock(*ifs->thenBlock);
        if (auto* elseBlock = dynamic_cast<AST::BlockStmt*>(ifs->elseBranch.get())) n += countSlotsInBlock(*elseBlock);
        if (auto* elseIf = dynamic_cast<AST::IfStmt*>(ifs->elseBranch.get())) n += countSlotsInStmt(*elseIf);
        return n;
    }
    if (auto* wh = dynamic_cast<AST::WhileStmt*>(&stmt)) return countSlotsInBlock(*wh->body);
    return 0;
}

void Generator::emitFunction(AST::FunctionDecl& fn) {
    std::string qualified = currentPrefix().empty() ? fn.name : currentPrefix() + "::" + fn.name;
    const std::string label = asmSymbolForQualifiedName(qualified);
    const int slots = countLocalSlots(fn);
    int frameSize = slots * 8;
    if (frameSize % 16 != 0) frameSize += 16 - (frameSize % 16);

    FunctionContext ctx;
    ctx.returnLabel = freshLabel("return_");
    ctx.pushScope();

    emit(label + ":");
    emit("    push rbp");
    emit("    mov rbp, rsp");
    if (frameSize > 0) emit("    sub rsp, " + std::to_string(frameSize));

    static const char* intArgRegs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
    if (fn.params.size() > 6) {
        addDiagnostic(fn, "codegen пока поддерживает не более 6 параметров функции по System V ABI");
    }
    for (std::size_t i = 0; i < fn.params.size(); ++i) {
        std::string type = fn.params[i].type ? "" : "";
        Local local = ctx.addLocal(fn.params[i].name, "int32");
        if (i < 6) emit(std::string("    mov [rbp") + std::to_string(local.offset) + "], " + intArgRegs[i]);
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
        emitExpr(*let->initializer, ctx);
        std::string type = exprType(*let->initializer);
        if (!isSupportedScalarType(type)) addDiagnostic(*let, "codegen пока поддерживает локальные переменные только скалярных типов, получен " + type);
        Local local = ctx.addLocal(let->name, type);
        emit("    mov [rbp" + std::to_string(local.offset) + "], rax");
        return;
    }
    if (auto* var = dynamic_cast<AST::VarStmt*>(&stmt)) {
        emitExpr(*var->initializer, ctx);
        std::string type = exprType(*var->initializer);
        if (!isSupportedScalarType(type)) addDiagnostic(*var, "codegen пока поддерживает локальные переменные только скалярных типов, получен " + type);
        Local local = ctx.addLocal(var->name, type);
        emit("    mov [rbp" + std::to_string(local.offset) + "], rax");
        return;
    }
    if (auto* asg = dynamic_cast<AST::AssignStmt*>(&stmt)) {
        emitExpr(*asg->value, ctx);
        emitStoreToLValue(*asg->target, ctx);
        return;
    }
    if (auto* exprStmt = dynamic_cast<AST::ExprStmt*>(&stmt)) {
        emitExpr(*exprStmt->expr, ctx);
        return;
    }
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
    if (auto* i = dynamic_cast<AST::IntLiteralExpr*>(&expr)) {
        emit("    mov rax, " + i->lexeme);
        return;
    }
    if (auto* f = dynamic_cast<AST::FloatLiteralExpr*>(&expr)) {
        addDiagnostic(*f, "codegen для float32/float64 ещё не реализован");
        emit("    xor rax, rax");
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
                emit("    mov rax, [rbp" + std::to_string(local->offset) + "]");
                return;
            }
        }
        addDiagnostic(expr, "codegen не может использовать имя как значение: " + joinPath(name->path));
        emit("    xor rax, rax");
        return;
    }
    if (auto* unary = dynamic_cast<AST::UnaryExpr*>(&expr)) {
        emitExpr(*unary->operand, ctx);
        if (unary->op == "-") emit("    neg rax");
        else if (unary->op == "!") {
            emit("    cmp rax, 0");
            emit("    sete al");
            emit("    movzx rax, al");
        }
        return;
    }
    if (auto* bin = dynamic_cast<AST::BinaryExpr*>(&expr)) return emitBinary(*bin, ctx);
    if (auto* cast = dynamic_cast<AST::CastExpr*>(&expr)) {
        emitExpr(*cast->value, ctx);
        // Все поддержанные скалярные значения передаются через RAX. Сужение/расширение
        // на уровне машинного представления будет доработано вместе с полным layout типов.
        return;
    }
    if (auto* call = dynamic_cast<AST::CallExpr*>(&expr)) return emitCall(*call, ctx);
    if (auto* arr = dynamic_cast<AST::ArrayLiteralExpr*>(&expr)) {
        addDiagnostic(*arr, "codegen для литералов массивов ещё не реализован");
        emit("    xor rax, rax");
        return;
    }
    if (auto* st = dynamic_cast<AST::StructLiteralExpr*>(&expr)) {
        addDiagnostic(*st, "codegen для литералов структур ещё не реализован");
        emit("    xor rax, rax");
        return;
    }
    if (auto* field = dynamic_cast<AST::FieldExpr*>(&expr)) {
        addDiagnostic(*field, "codegen для доступа к полям структур ещё не реализован");
        emit("    xor rax, rax");
        return;
    }
    if (auto* idx = dynamic_cast<AST::IndexExpr*>(&expr)) {
        addDiagnostic(*idx, "codegen для индексирования массивов ещё не реализован");
        emit("    xor rax, rax");
        return;
    }
    addDiagnostic(expr, "неизвестное выражение для codegen");
    emit("    xor rax, rax");
}

void Generator::emitBinary(AST::BinaryExpr& expr, FunctionContext& ctx) {
    if (expr.op == "&&") return emitLogicalAnd(expr, ctx);
    if (expr.op == "||") return emitLogicalOr(expr, ctx);
    const std::string type = exprType(expr);
    if (type == "string") {
        addDiagnostic(expr, "codegen для строковых операций пока поддерживает только print/string literal");
        emit("    xor rax, rax");
        return;
    }
    emitExpr(*expr.left, ctx);
    emit("    push rax");
    emitExpr(*expr.right, ctx);
    emit("    mov rbx, rax");
    emit("    pop rax");

    const auto& op = expr.op;
    if (op == "+") emit("    add rax, rbx");
    else if (op == "-") emit("    sub rax, rbx");
    else if (op == "*") emit("    imul rax, rbx");
    else if (op == "/" || op == "%") {
        const std::string okLabel = freshLabel("div_ok_");
        emit("    cmp rbx, 0");
        emit("    jne " + okLabel);
        emit("    mov rdi, " + std::to_string(expr.span.begin.line));
        emit("    call astra_rt_div_zero");
        emit(okLabel + ":");
        emit("    cqo");
        emit("    idiv rbx");
        if (op == "%") emit("    mov rax, rdx");
    } else if (op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=") {
        emit("    cmp rax, rbx");
        if (op == "==") emit("    sete al");
        else if (op == "!=") emit("    setne al");
        else if (op == "<") emit("    setl al");
        else if (op == "<=") emit("    setle al");
        else if (op == ">") emit("    setg al");
        else emit("    setge al");
        emit("    movzx rax, al");
    } else {
        addDiagnostic(expr, "неизвестный бинарный оператор в codegen: " + op);
    }
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
        emitExpr(*expr.args[0], ctx);
        emit("    mov rdi, rax");
        const std::string t = exprType(*expr.args[0]);
        if (t == "string") emit("    call astra_print_string");
        else if (t == "bool") emit("    call astra_print_bool");
        else if (t == "int64" || t == "uint64") emit("    call astra_print_i64");
        else emit("    call astra_print_i32");
        emit("    xor rax, rax");
        return;
    }
    if (calleeName->path.size() == 1 && name == "input") {
        emit("    call astra_input_string");
        return;
    }
    if (calleeName->path.size() == 1 && name == "len") {
        if (expr.args.size() == 1) {
            emitExpr(*expr.args[0], ctx);
            emit("    mov rdi, rax");
            emit("    call astra_string_len");
        }
        return;
    }
    if (calleeName->path.size() == 1 && name == "exit") {
        if (expr.args.size() == 1) {
            emitExpr(*expr.args[0], ctx);
            emit("    mov rdi, rax");
            emit("    call astra_exit");
        }
        return;
    }
    if (calleeName->path.size() == 1 && name == "panic") {
        if (expr.args.size() == 1) {
            emitExpr(*expr.args[0], ctx);
            emit("    mov rdi, rax");
            emit("    call astra_panic");
        }
        return;
    }

    if (expr.args.size() > 6) addDiagnostic(expr, "codegen пока поддерживает вызовы максимум с 6 аргументами");
    static const char* intArgRegs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
    const std::size_t common = std::min<std::size_t>(expr.args.size(), 6);
    for (std::size_t i = 0; i < common; ++i) {
        emitExpr(*expr.args[i], ctx);
        emit("    push rax");
    }
    for (std::size_t i = common; i > 0; --i) {
        emit(std::string("    pop ") + intArgRegs[i - 1]);
    }
    emit("    call " + asmSymbolForPath(calleeName->path));
}

void Generator::emitStoreToLValue(AST::Expr& target, FunctionContext& ctx) {
    if (auto* name = dynamic_cast<AST::NameExpr*>(&target)) {
        if (name->path.size() == 1) {
            if (const Local* local = ctx.findLocal(name->path[0])) {
                emit("    mov [rbp" + std::to_string(local->offset) + "], rax");
                return;
            }
        }
    }
    addDiagnostic(target, "codegen пока поддерживает присваивание только в локальную переменную");
}

std::string formatDiagnostic(const Diagnostic& diagnostic) {
    return diagnostic.file + ":" + std::to_string(diagnostic.pos.line) + ":" +
           std::to_string(diagnostic.pos.column) + ": error: " + diagnostic.message;
}

} // namespace Codegen
