#include "semantic.h"

#include <algorithm>
#include <charconv>
#include <limits>
#include <memory>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace Semantic {

Type Type::error() {
    Type t;
    t.kind = Kind::Error;
    t.name = "<error>";
    return t;
}

Type Type::unit() {
    Type t;
    t.kind = Kind::Unit;
    t.name = "unit";
    return t;
}

Type Type::boolean() {
    Type t;
    t.kind = Kind::Bool;
    t.name = "bool";
    return t;
}

Type Type::string() {
    Type t;
    t.kind = Kind::String;
    t.name = "string";
    return t;
}

Type Type::integer(std::string name, int bits, bool isUnsigned) {
    Type t;
    t.kind = isUnsigned ? Kind::UInt : Kind::Int;
    t.bits = bits;
    t.name = std::move(name);
    return t;
}

Type Type::floating(std::string name, int bits) {
    Type t;
    t.kind = Kind::Float;
    t.bits = bits;
    t.name = std::move(name);
    return t;
}

Type Type::array(Type element, std::uint64_t size) {
    auto ptr = std::make_shared<Type>(std::move(element));
    Type t;
    t.kind = Kind::Array;
    t.elementType = ptr;
    t.arraySize = size;
    t.name = "[" + ptr->toString() + "; " + std::to_string(size) + "]";
    return t;
}

Type Type::structure(std::string qualifiedName) {
    Type t;
    t.kind = Kind::Struct;
    t.name = std::move(qualifiedName);
    return t;
}

bool Type::operator==(const Type& other) const {
    if (kind != other.kind) return false;
    if (kind == Kind::Array) {
        if (arraySize != other.arraySize) return false;
        if (!elementType || !other.elementType) return false;
        return *elementType == *other.elementType;
    }
    if (kind == Kind::Int || kind == Kind::UInt || kind == Kind::Float) {
        return bits == other.bits && name == other.name;
    }
    return name == other.name;
}

std::string Type::toString() const {
    if (kind == Kind::Array) {
        const std::string elem = elementType ? elementType->toString() : "<error>";
        return "[" + elem + "; " + std::to_string(arraySize) + "]";
    }
    return name;
}

Analyzer::Analyzer(std::string fileName) : fileName_(std::move(fileName)) {}

Analyzer::Scope* Analyzer::makeScope(Scope* parent, bool isNamespace, std::string qualifiedName) {
    auto scope = std::make_unique<Scope>();
    scope->parent = parent;
    scope->isNamespace = isNamespace;
    scope->qualifiedName = std::move(qualifiedName);
    Scope* raw = scope.get();
    ownedScopes_.push_back(std::move(scope));
    return raw;
}

std::string Analyzer::qualify(std::string_view name) const {
    if (namespaceStack_.empty()) return std::string(name);
    std::string result;
    for (const auto& ns : namespaceStack_) {
        if (!result.empty()) result += "::";
        result += ns;
    }
    result += "::";
    result += name;
    return result;
}

std::string Analyzer::joinPath(const std::vector<std::string>& path) const {
    std::string result;
    for (const auto& p : path) {
        if (!result.empty()) result += "::";
        result += p;
    }
    return result;
}

void Analyzer::addDiagnostic(const AST::Node& node, std::string message) {
    addDiagnostic(node.span.begin, std::move(message));
}

void Analyzer::addDiagnostic(Lexer::Position pos, std::string message) {
    diagnostics_.push_back(Diagnostic{fileName_, pos, std::move(message)});
}

bool Analyzer::analyze(AST::Module& module) {
    diagnostics_.clear();
    ownedScopes_.clear();
    namespaceStack_.clear();
    currentReturnType_ = Type::unit();
    loopDepth_ = 0;

    rootScope_ = makeScope(nullptr, true, "");
    currentScope_ = rootScope_;
    installBuiltins();

    for (auto& decl : module.decls) {
        analyzeDecl(*decl);
    }

    auto* mainSym = lookupLocal(*rootScope_, "main");
    if (!mainSym || mainSym->kind != SymbolKind::Function || !mainSym->function) {
        addDiagnostic(module, "отсутствует точка входа fn main() -> int32");
    } else {
        const auto& fn = *mainSym->function;
        if (!fn.paramTypes.empty() || fn.returnType != Type::integer("int32", 32, false)) {
            addDiagnostic(*fn.decl, "main должна иметь сигнатуру fn main() -> int32");
        }
    }

    return diagnostics_.empty();
}

void Analyzer::installBuiltins() {
    addBuiltinFunction("input", Type::string());
    addBuiltinFunction("exit", Type::unit(), {Type::integer("int32", 32, false)});
    addBuiltinFunction("panic", Type::unit(), {Type::string()});
    addBuiltinFunction("len", Type::integer("int32", 32, false), {Type::string()});

    auto printInfo = std::make_shared<FunctionInfo>();
    printInfo->name = "print";
    printInfo->qualifiedName = "print";
    printInfo->returnType = Type::unit();
    printInfo->isBuiltin = true;
    printInfo->builtinName = "print";
    Symbol printSym;
    printSym.kind = SymbolKind::Function;
    printSym.name = "print";
    printSym.function = printInfo;
    rootScope_->symbols.emplace("print", std::move(printSym));
}

void Analyzer::addBuiltinFunction(std::string name, Type returnType, std::vector<Type> params) {
    auto info = std::make_shared<FunctionInfo>();
    info->name = name;
    info->qualifiedName = name;
    info->paramTypes = std::move(params);
    info->returnType = std::move(returnType);
    info->isBuiltin = true;
    info->builtinName = name;

    Symbol sym;
    sym.kind = SymbolKind::Function;
    sym.name = name;
    sym.function = info;
    rootScope_->symbols.emplace(name, std::move(sym));
}

bool Analyzer::declare(Scope& scope, const Symbol& symbol, const AST::Node& node) {
    if (scope.symbols.contains(symbol.name)) {
        addDiagnostic(node, "повторное объявление имени '" + symbol.name + "' в одной области видимости");
        return false;
    }
    scope.symbols.emplace(symbol.name, symbol);
    return true;
}

Analyzer::Symbol* Analyzer::lookupLocal(Scope& scope, const std::string& name) {
    auto it = scope.symbols.find(name);
    if (it == scope.symbols.end()) return nullptr;
    return &it->second;
}

Analyzer::Symbol* Analyzer::lookupLexical(const std::string& name) {
    for (Scope* scope = currentScope_; scope != nullptr; scope = scope->parent) {
        if (auto* sym = lookupLocal(*scope, name)) return sym;
    }
    return nullptr;
}

Analyzer::Symbol* Analyzer::resolvePath(const std::vector<std::string>& path, const AST::Node& node) {
    if (path.empty()) return nullptr;
    Symbol* sym = lookupLexical(path.front());
    if (!sym) {
        addDiagnostic(node, "использование необъявленного идентификатора '" + path.front() + "'");
        return nullptr;
    }

    for (std::size_t i = 1; i < path.size(); ++i) {
        if (sym->kind != SymbolKind::Namespace || !sym->namespaceScope) {
            addDiagnostic(node, "'" + sym->name + "' не является пространством имён");
            return nullptr;
        }
        sym = lookupLocal(*sym->namespaceScope, path[i]);
        if (!sym) {
            addDiagnostic(node, "имя '" + path[i] + "' не найдено в '" + joinPath(std::vector<std::string>(path.begin(), path.begin() + static_cast<std::ptrdiff_t>(i))) + "'");
            return nullptr;
        }
    }
    return sym;
}

Analyzer::Symbol* Analyzer::resolvePathFromScope(Scope& start, const std::vector<std::string>& path, const AST::Node& node) {
    if (path.empty()) return nullptr;
    Symbol* sym = lookupLocal(start, path.front());
    if (!sym) {
        addDiagnostic(node, "использование необъявленного идентификатора '" + path.front() + "'");
        return nullptr;
    }
    for (std::size_t i = 1; i < path.size(); ++i) {
        if (sym->kind != SymbolKind::Namespace || !sym->namespaceScope) {
            addDiagnostic(node, "'" + sym->name + "' не является пространством имён");
            return nullptr;
        }
        sym = lookupLocal(*sym->namespaceScope, path[i]);
        if (!sym) {
            addDiagnostic(node, "имя '" + path[i] + "' не найдено");
            return nullptr;
        }
    }
    return sym;
}

void Analyzer::analyzeDecl(AST::Decl& decl) {
    if (auto* ns = dynamic_cast<AST::NamespaceDecl*>(&decl)) return analyzeNamespaceDecl(*ns);
    if (auto* alias = dynamic_cast<AST::TypeAliasDecl*>(&decl)) return analyzeTypeAliasDecl(*alias);
    if (auto* st = dynamic_cast<AST::StructDecl*>(&decl)) return analyzeStructDecl(*st);
    if (auto* fn = dynamic_cast<AST::FunctionDecl*>(&decl)) return analyzeFunctionDecl(*fn);
    addDiagnostic(decl, "неизвестный вид объявления");
}

void Analyzer::analyzeNamespaceDecl(AST::NamespaceDecl& decl) {
    auto* nsScope = makeScope(currentScope_, true, qualify(decl.name));
    Symbol sym;
    sym.kind = SymbolKind::Namespace;
    sym.name = decl.name;
    sym.namespaceScope = nsScope;
    if (!declare(*currentScope_, sym, decl)) return;

    Scope* saved = currentScope_;
    currentScope_ = nsScope;
    namespaceStack_.push_back(decl.name);
    for (auto& child : decl.decls) analyzeDecl(*child);
    namespaceStack_.pop_back();
    currentScope_ = saved;
}

void Analyzer::analyzeTypeAliasDecl(AST::TypeAliasDecl& decl) {
    Type target = resolveType(*decl.aliasedType);
    Symbol sym;
    sym.kind = SymbolKind::Alias;
    sym.name = decl.name;
    sym.type = target;
    declare(*currentScope_, sym, decl);
}

void Analyzer::analyzeStructDecl(AST::StructDecl& decl) {
    auto info = std::make_shared<StructInfo>();
    info->name = decl.name;
    info->qualifiedName = qualify(decl.name);

    std::unordered_set<std::string> seenFields;
    for (auto& field : decl.fields) {
        if (!seenFields.insert(field.name).second) {
            addDiagnostic(field.span.begin, "повторное поле структуры '" + field.name + "'");
            continue;
        }
        Type fieldType = resolveType(*field.type);
        info->fields.emplace(field.name, fieldType);
        info->fieldsInOrder.emplace_back(field.name, fieldType);
    }

    Symbol sym;
    sym.kind = SymbolKind::Struct;
    sym.name = decl.name;
    sym.type = Type::structure(info->qualifiedName);
    sym.structure = info;
    declare(*currentScope_, sym, decl);
}

void Analyzer::analyzeFunctionDecl(AST::FunctionDecl& decl) {
    auto info = std::make_shared<FunctionInfo>();
    info->name = decl.name;
    info->qualifiedName = qualify(decl.name);
    info->decl = &decl;

    std::unordered_set<std::string> paramNames;
    for (auto& param : decl.params) {
        if (!paramNames.insert(param.name).second) {
            addDiagnostic(param.span.begin, "повторный параметр функции '" + param.name + "'");
        }
        info->paramTypes.push_back(resolveType(*param.type));
    }
    info->returnType = resolveType(*decl.returnType);

    Symbol fnSym;
    fnSym.kind = SymbolKind::Function;
    fnSym.name = decl.name;
    fnSym.function = info;
    if (!declare(*currentScope_, fnSym, decl)) return;

    Scope* savedScope = currentScope_;
    Type savedReturn = currentReturnType_;
    int savedLoopDepth = loopDepth_;

    auto* functionScope = makeScope(currentScope_, false, info->qualifiedName + "::<fn>");
    currentScope_ = functionScope;
    currentReturnType_ = info->returnType;
    loopDepth_ = 0;

    for (std::size_t i = 0; i < decl.params.size(); ++i) {
        Symbol paramSym;
        paramSym.kind = SymbolKind::Variable;
        paramSym.name = decl.params[i].name;
        paramSym.type = info->paramTypes[i];
        paramSym.isMutable = false;
        declare(*currentScope_, paramSym, *decl.params[i].type);
    }

    Flow flow = analyzeBlock(*decl.body, false);
    if (info->returnType.kind != Type::Kind::Unit && flow == Flow::MayContinue) {
        addDiagnostic(decl, "не все пути исполнения функции '" + decl.name + "' возвращают значение");
    }

    currentScope_ = savedScope;
    currentReturnType_ = savedReturn;
    loopDepth_ = savedLoopDepth;
}

Type Analyzer::resolveType(AST::TypeExpr& typeExpr) {
    if (auto* named = dynamic_cast<AST::NamedType*>(&typeExpr)) return resolveNamedType(*named);
    if (auto* arr = dynamic_cast<AST::ArrayType*>(&typeExpr)) {
        Type elem = resolveType(*arr->elementType);
        if (arr->size == 0) {
            addDiagnostic(*arr, "размер массива должен быть положительным");
            return Type::error();
        }
        return Type::array(elem, arr->size);
    }
    addDiagnostic(typeExpr, "неизвестное выражение типа");
    return Type::error();
}

Type Analyzer::resolveNamedType(AST::NamedType& typeExpr) {
    if (typeExpr.path.size() == 1) {
        const std::string& name = typeExpr.path[0];
        if (name == "unit") return Type::unit();
        if (name == "bool") return Type::boolean();
        if (name == "string") return Type::string();
        if (name == "int8") return Type::integer("int8", 8, false);
        if (name == "int16") return Type::integer("int16", 16, false);
        if (name == "int32") return Type::integer("int32", 32, false);
        if (name == "int64") return Type::integer("int64", 64, false);
        if (name == "uint8") return Type::integer("uint8", 8, true);
        if (name == "uint16") return Type::integer("uint16", 16, true);
        if (name == "uint32") return Type::integer("uint32", 32, true);
        if (name == "uint64") return Type::integer("uint64", 64, true);
        if (name == "float32") return Type::floating("float32", 32);
        if (name == "float64") return Type::floating("float64", 64);
    }

    Symbol* sym = resolvePath(typeExpr.path, typeExpr);
    if (!sym) return Type::error();
    if (sym->kind == SymbolKind::Alias) return sym->type;
    if (sym->kind == SymbolKind::Struct) return sym->type;
    addDiagnostic(typeExpr, "'" + joinPath(typeExpr.path) + "' не является типом");
    return Type::error();
}

Analyzer::Flow Analyzer::analyzeBlock(AST::BlockStmt& block, bool createScope) {
    Scope* saved = currentScope_;
    if (createScope) currentScope_ = makeScope(currentScope_, false, "<block>");

    Flow flow = Flow::MayContinue;
    for (auto& stmt : block.statements) {
        if (flow == Flow::NoContinue) {
            // По спецификации это можно считать предупреждением; чтобы не ломать базовые программы,
            // здесь не выдаём error, но поток выполнения уже известен.
            analyzeStmt(*stmt);
            continue;
        }
        Flow stmtFlow = analyzeStmt(*stmt);
        if (stmtFlow == Flow::NoContinue) flow = Flow::NoContinue;
    }

    if (createScope) currentScope_ = saved;
    return flow;
}

Analyzer::Flow Analyzer::analyzeStmt(AST::Stmt& stmt) {
    if (dynamic_cast<AST::EmptyStmt*>(&stmt)) return Flow::MayContinue;
    if (auto* block = dynamic_cast<AST::BlockStmt*>(&stmt)) return analyzeBlock(*block, true);

    if (auto* let = dynamic_cast<AST::LetStmt*>(&stmt)) {
        std::optional<Type> expected;
        Type declaredType = Type::error();
        if (let->explicitType) {
            declaredType = resolveType(*let->explicitType);
            expected = declaredType;
        }
        Type initType = analyzeExpr(*let->initializer, expected);
        Type variableType = let->explicitType ? declaredType : initType;
        if (let->explicitType) checkAssignable(declaredType, initType, *let);

        Symbol sym;
        sym.kind = SymbolKind::Variable;
        sym.name = let->name;
        sym.type = variableType;
        sym.isMutable = false;
        declare(*currentScope_, sym, *let);
        return Flow::MayContinue;
    }

    if (auto* var = dynamic_cast<AST::VarStmt*>(&stmt)) {
        Type declaredType = resolveType(*var->explicitType);
        Type initType = analyzeExpr(*var->initializer, declaredType);
        checkAssignable(declaredType, initType, *var);

        Symbol sym;
        sym.kind = SymbolKind::Variable;
        sym.name = var->name;
        sym.type = declaredType;
        sym.isMutable = true;
        declare(*currentScope_, sym, *var);
        return Flow::MayContinue;
    }

    if (auto* assign = dynamic_cast<AST::AssignStmt*>(&stmt)) {
        LValueInfo target = analyzeLValue(*assign->target);
        Type rhs = analyzeExpr(*assign->value, target.type);
        if (!target.isLValue) {
            addDiagnostic(*assign->target, "левая часть присваивания не является lvalue");
        } else if (!target.isMutable) {
            addDiagnostic(*assign->target, "нельзя присвоить значение в неизменяемый let/параметр");
        }
        checkAssignable(target.type, rhs, *assign);
        return Flow::MayContinue;
    }

    if (auto* expr = dynamic_cast<AST::ExprStmt*>(&stmt)) return analyzeExprStmt(*expr);
    if (auto* ifStmt = dynamic_cast<AST::IfStmt*>(&stmt)) return analyzeIf(*ifStmt);
    if (auto* whileStmt = dynamic_cast<AST::WhileStmt*>(&stmt)) return analyzeWhile(*whileStmt);

    if (auto* ret = dynamic_cast<AST::ReturnStmt*>(&stmt)) {
        if (!ret->value) {
            if (currentReturnType_.kind != Type::Kind::Unit) {
                addDiagnostic(*ret, "return; допустим только в функции с результатом unit");
            }
        } else {
            Type valueType = analyzeExpr(*ret->value, currentReturnType_);
            checkAssignable(currentReturnType_, valueType, *ret);
        }
        return Flow::NoContinue;
    }

    if (dynamic_cast<AST::BreakStmt*>(&stmt)) {
        if (loopDepth_ == 0) addDiagnostic(stmt, "break вне цикла");
        return Flow::NoContinue;
    }

    if (dynamic_cast<AST::ContinueStmt*>(&stmt)) {
        if (loopDepth_ == 0) addDiagnostic(stmt, "continue вне цикла");
        return Flow::NoContinue;
    }

    addDiagnostic(stmt, "неизвестный вид инструкции");
    return Flow::MayContinue;
}

Analyzer::Flow Analyzer::analyzeIf(AST::IfStmt& stmt) {
    Type cond = analyzeExpr(*stmt.condition, Type::boolean());
    checkAssignable(Type::boolean(), cond, *stmt.condition);

    Flow thenFlow = analyzeBlock(*stmt.thenBlock, true);
    Flow elseFlow = Flow::MayContinue;
    if (stmt.elseBranch) {
        elseFlow = analyzeStmt(*stmt.elseBranch);
    }
    if (stmt.elseBranch && thenFlow == Flow::NoContinue && elseFlow == Flow::NoContinue) {
        return Flow::NoContinue;
    }
    return Flow::MayContinue;
}

Analyzer::Flow Analyzer::analyzeWhile(AST::WhileStmt& stmt) {
    Type cond = analyzeExpr(*stmt.condition, Type::boolean());
    checkAssignable(Type::boolean(), cond, *stmt.condition);
    ++loopDepth_;
    analyzeBlock(*stmt.body, true);
    --loopDepth_;
    return Flow::MayContinue;
}

Analyzer::Flow Analyzer::analyzeExprStmt(AST::ExprStmt& stmt) {
    analyzeExpr(*stmt.expr);
    if (isTerminatingCall(*stmt.expr)) return Flow::NoContinue;
    return Flow::MayContinue;
}

Type Analyzer::analyzeExpr(AST::Expr& expr, const std::optional<Type>& expected) {
    Type result = Type::error();

    if (auto* e = dynamic_cast<AST::IntLiteralExpr*>(&expr)) {
        (void)e;
        result = Type::integer("int32", 32, false);
    } else if (auto* e = dynamic_cast<AST::FloatLiteralExpr*>(&expr)) {
        (void)e;
        result = Type::floating("float64", 64);
    } else if (auto* e = dynamic_cast<AST::BoolLiteralExpr*>(&expr)) {
        (void)e;
        result = Type::boolean();
    } else if (auto* e = dynamic_cast<AST::StringLiteralExpr*>(&expr)) {
        (void)e;
        result = Type::string();
    } else if (auto* e = dynamic_cast<AST::NameExpr*>(&expr)) {
        result = analyzeNameExpr(*e);
    } else if (auto* e = dynamic_cast<AST::ArrayLiteralExpr*>(&expr)) {
        result = analyzeArrayLiteral(*e, expected);
    } else if (auto* e = dynamic_cast<AST::StructLiteralExpr*>(&expr)) {
        result = analyzeStructLiteral(*e);
    } else if (auto* e = dynamic_cast<AST::UnaryExpr*>(&expr)) {
        result = analyzeUnary(*e);
    } else if (auto* e = dynamic_cast<AST::BinaryExpr*>(&expr)) {
        result = analyzeBinary(*e);
    } else if (auto* e = dynamic_cast<AST::CastExpr*>(&expr)) {
        result = analyzeCast(*e);
    } else if (auto* e = dynamic_cast<AST::CallExpr*>(&expr)) {
        result = analyzeCall(*e);
    } else if (auto* e = dynamic_cast<AST::FieldExpr*>(&expr)) {
        result = analyzeField(*e);
    } else if (auto* e = dynamic_cast<AST::IndexExpr*>(&expr)) {
        result = analyzeIndex(*e);
    } else {
        addDiagnostic(expr, "неизвестный вид выражения");
    }

    expr.semanticType = result.toString();
    return result;
}

Type Analyzer::analyzeNameExpr(AST::NameExpr& expr) {
    Symbol* sym = resolvePath(expr.path, expr);
    if (!sym) return Type::error();
    if (sym->kind == SymbolKind::Variable) return sym->type;
    if (sym->kind == SymbolKind::Function) {
        addDiagnostic(expr, "функции не являются значениями первого класса; используйте вызов функции");
        return Type::error();
    }
    if (sym->kind == SymbolKind::Struct || sym->kind == SymbolKind::Alias) {
        addDiagnostic(expr, "тип нельзя использовать как значение");
        return Type::error();
    }
    if (sym->kind == SymbolKind::Namespace) {
        addDiagnostic(expr, "пространство имён нельзя использовать как значение");
        return Type::error();
    }
    return Type::error();
}

Type Analyzer::analyzeArrayLiteral(AST::ArrayLiteralExpr& expr, const std::optional<Type>& expected) {
    std::optional<Type> elemExpected;
    std::uint64_t expectedSize = 0;
    if (expected && expected->kind == Type::Kind::Array) {
        elemExpected = *expected->elementType;
        expectedSize = expected->arraySize;
    }

    if (expr.elements.empty()) {
        if (!elemExpected) {
            addDiagnostic(expr, "пустой литерал массива без ожидаемого типа запрещён");
            return Type::error();
        }
        if (expectedSize != 0) {
            addDiagnostic(expr, "размер литерала массива 0 не совпадает с ожидаемым размером " + std::to_string(expectedSize));
        }
        return Type::array(*elemExpected, 0);
    }

    Type elemType = analyzeExpr(*expr.elements.front(), elemExpected);
    for (std::size_t i = 1; i < expr.elements.size(); ++i) {
        Type current = analyzeExpr(*expr.elements[i], elemExpected ? elemExpected : elemType);
        checkAssignable(elemExpected ? *elemExpected : elemType, current, *expr.elements[i]);
    }

    Type result = Type::array(elemExpected ? *elemExpected : elemType, expr.elements.size());
    if (expected && expected->kind == Type::Kind::Array && expected->arraySize != expr.elements.size()) {
        addDiagnostic(expr, "размер литерала массива " + std::to_string(expr.elements.size()) +
                            " не совпадает с ожидаемым размером " + std::to_string(expected->arraySize));
    }
    return result;
}

Type Analyzer::analyzeStructLiteral(AST::StructLiteralExpr& expr) {
    AST::NamedType fakeType;
    fakeType.path = expr.typePath;
    fakeType.span = expr.span;
    Type type = resolveNamedType(fakeType);
    if (type.kind != Type::Kind::Struct) {
        addDiagnostic(expr, "литерал структуры требует имя структуры");
        return Type::error();
    }

    Symbol* sym = resolvePath(expr.typePath, expr);
    if (!sym || sym->kind != SymbolKind::Struct || !sym->structure) return Type::error();
    const auto& st = *sym->structure;

    std::unordered_set<std::string> seen;
    for (auto& field : expr.fields) {
        if (!seen.insert(field.name).second) {
            addDiagnostic(field.span.begin, "поле '" + field.name + "' инициализировано повторно");
            continue;
        }
        auto it = st.fields.find(field.name);
        if (it == st.fields.end()) {
            addDiagnostic(field.span.begin, "лишнее поле '" + field.name + "' в литерале структуры '" + st.qualifiedName + "'");
            analyzeExpr(*field.value);
            continue;
        }
        Type valueType = analyzeExpr(*field.value, it->second);
        checkAssignable(it->second, valueType, *field.value);
    }

    for (const auto& [name, typeOfField] : st.fieldsInOrder) {
        (void)typeOfField;
        if (!seen.contains(name)) {
            addDiagnostic(expr, "не инициализировано поле '" + name + "' структуры '" + st.qualifiedName + "'");
        }
    }

    return type;
}

Type Analyzer::analyzeUnary(AST::UnaryExpr& expr) {
    Type operand = analyzeExpr(*expr.operand);
    if (expr.op == "-") {
        if (!operand.isNumeric()) {
            addDiagnostic(expr, "унарный '-' применим только к числовым типам");
            return Type::error();
        }
        return operand;
    }
    if (expr.op == "!") {
        if (operand != Type::boolean()) {
            addDiagnostic(expr, "оператор '!' применим только к bool");
            return Type::error();
        }
        return Type::boolean();
    }
    addDiagnostic(expr, "неизвестный унарный оператор '" + expr.op + "'");
    return Type::error();
}

Type Analyzer::analyzeBinary(AST::BinaryExpr& expr) {
    Type left = analyzeExpr(*expr.left);
    Type right = analyzeExpr(*expr.right, left);
    const std::string& op = expr.op;

    if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%") {
        if (op == "+" && left == Type::string() && right == Type::string()) return Type::string();
        if (op == "%") {
            if (!left.isInteger() || !right.isInteger()) {
                addDiagnostic(expr, "оператор '%' применим только к целочисленным типам");
                return Type::error();
            }
        } else if (!left.isNumeric() || !right.isNumeric()) {
            addDiagnostic(expr, "арифметический оператор '" + op + "' применим только к числовым типам");
            return Type::error();
        }
        checkAssignable(left, right, expr);
        return left;
    }

    if (op == "&&" || op == "||") {
        checkAssignable(Type::boolean(), left, *expr.left);
        checkAssignable(Type::boolean(), right, *expr.right);
        return Type::boolean();
    }

    if (op == "==" || op == "!=") {
        checkAssignable(left, right, expr);
        if (!(left.isNumeric() || left.kind == Type::Kind::Bool || left.kind == Type::Kind::String ||
              left.kind == Type::Kind::Array || left.kind == Type::Kind::Struct || left.isError())) {
            addDiagnostic(expr, "оператор '" + op + "' не определён для типа " + left.toString());
        }
        return Type::boolean();
    }

    if (op == "<" || op == "<=" || op == ">" || op == ">=") {
        if (!left.isNumeric() || !right.isNumeric()) {
            addDiagnostic(expr, "порядковые сравнения применимы только к числовым типам");
        }
        checkAssignable(left, right, expr);
        return Type::boolean();
    }

    addDiagnostic(expr, "неизвестный бинарный оператор '" + op + "'");
    return Type::error();
}

Type Analyzer::analyzeCast(AST::CastExpr& expr) {
    Type from = analyzeExpr(*expr.value);
    Type to = resolveType(*expr.targetType);
    if (!canCast(from, to)) {
        addDiagnostic(expr, "недопустимое приведение из " + from.toString() + " в " + to.toString());
        return Type::error();
    }
    return to;
}

Type Analyzer::analyzeCall(AST::CallExpr& expr) {
    auto* calleeName = dynamic_cast<AST::NameExpr*>(expr.callee.get());
    if (!calleeName) {
        analyzeExpr(*expr.callee);
        for (auto& arg : expr.args) analyzeExpr(*arg);
        addDiagnostic(expr, "вызов возможен только для имени функции");
        return Type::error();
    }

    Symbol* sym = resolvePath(calleeName->path, *calleeName);
    if (!sym) {
        for (auto& arg : expr.args) analyzeExpr(*arg);
        return Type::error();
    }
    if (sym->kind != SymbolKind::Function || !sym->function) {
        for (auto& arg : expr.args) analyzeExpr(*arg);
        addDiagnostic(expr, "вызов не-функции");
        return Type::error();
    }

    const auto& fn = *sym->function;
    if (fn.isBuiltin && fn.builtinName == "print") {
        if (expr.args.size() != 1) {
            addDiagnostic(expr, "print ожидает ровно один аргумент");
            for (auto& arg : expr.args) analyzeExpr(*arg);
            return Type::unit();
        }
        Type arg = analyzeExpr(*expr.args[0]);
        if (!isPrintable(arg)) {
            addDiagnostic(*expr.args[0], "тип " + arg.toString() + " нельзя напечатать в базовой реализации");
        }
        expr.callee->semanticType = "fn(printable) -> unit";
        return Type::unit();
    }

    if (expr.args.size() != fn.paramTypes.size()) {
        addDiagnostic(expr, "функция '" + fn.qualifiedName + "' ожидает " + std::to_string(fn.paramTypes.size()) +
                            " аргумент(ов), получено " + std::to_string(expr.args.size()));
    }

    const std::size_t common = std::min(expr.args.size(), fn.paramTypes.size());
    for (std::size_t i = 0; i < common; ++i) {
        Type arg = analyzeExpr(*expr.args[i], fn.paramTypes[i]);
        checkAssignable(fn.paramTypes[i], arg, *expr.args[i]);
    }
    for (std::size_t i = common; i < expr.args.size(); ++i) {
        analyzeExpr(*expr.args[i]);
    }

    expr.callee->semanticType = "fn -> " + fn.returnType.toString();
    return fn.returnType;
}

Type Analyzer::analyzeField(AST::FieldExpr& expr) {
    Type object = analyzeExpr(*expr.object);
    if (object.kind != Type::Kind::Struct) {
        addDiagnostic(expr, "доступ к полю возможен только у структуры");
        return Type::error();
    }

    std::vector<std::string> path;
    std::size_t start = 0;
    while (start < object.name.size()) {
        std::size_t pos = object.name.find("::", start);
        if (pos == std::string::npos) {
            path.push_back(object.name.substr(start));
            break;
        }
        path.push_back(object.name.substr(start, pos - start));
        start = pos + 2;
    }

    Symbol* sym = resolvePath(path, expr);
    if (!sym || sym->kind != SymbolKind::Struct || !sym->structure) return Type::error();
    auto it = sym->structure->fields.find(expr.field);
    if (it == sym->structure->fields.end()) {
        addDiagnostic(expr, "структура '" + object.name + "' не содержит поле '" + expr.field + "'");
        return Type::error();
    }
    return it->second;
}

Type Analyzer::analyzeIndex(AST::IndexExpr& expr) {
    Type object = analyzeExpr(*expr.object);
    Type index = analyzeExpr(*expr.index);
    if (object.kind != Type::Kind::Array) {
        addDiagnostic(expr, "индексирование возможно только для массива");
        return Type::error();
    }
    if (!index.isInteger()) {
        addDiagnostic(*expr.index, "индекс массива должен иметь целочисленный тип");
    }
    return object.elementType ? *object.elementType : Type::error();
}

Analyzer::LValueInfo Analyzer::analyzeLValue(AST::Expr& expr) {
    if (auto* name = dynamic_cast<AST::NameExpr*>(&expr)) {
        Symbol* sym = resolvePath(name->path, *name);
        if (!sym) return {};
        if (sym->kind != SymbolKind::Variable) {
            addDiagnostic(expr, "левая часть присваивания должна быть переменной, полем или элементом массива");
            return {Type::error(), false, false};
        }
        expr.semanticType = sym->type.toString();
        return {sym->type, true, sym->isMutable};
    }

    if (auto* field = dynamic_cast<AST::FieldExpr*>(&expr)) {
        LValueInfo base = analyzeLValue(*field->object);
        if (base.type.kind != Type::Kind::Struct) {
            addDiagnostic(expr, "доступ к полю возможен только у структуры");
            return {Type::error(), false, false};
        }
        Type fieldType = analyzeField(*field);
        expr.semanticType = fieldType.toString();
        return {fieldType, true, base.isMutable};
    }

    if (auto* index = dynamic_cast<AST::IndexExpr*>(&expr)) {
        LValueInfo base = analyzeLValue(*index->object);
        Type indexType = analyzeExpr(*index->index);
        if (!indexType.isInteger()) addDiagnostic(*index->index, "индекс массива должен иметь целочисленный тип");
        if (base.type.kind != Type::Kind::Array) {
            addDiagnostic(expr, "индексирование возможно только для массива");
            return {Type::error(), false, false};
        }
        Type elem = index->object->semanticType ? (base.type.elementType ? *base.type.elementType : Type::error()) : Type::error();
        expr.semanticType = elem.toString();
        return {elem, true, base.isMutable};
    }

    analyzeExpr(expr);
    return {Type::error(), false, false};
}

bool Analyzer::checkAssignable(const Type& lhs, const Type& rhs, const AST::Node& node) {
    if (lhs.isError() || rhs.isError()) return false;
    if (lhs != rhs) {
        addDiagnostic(node, "несовместимые типы: ожидался " + lhs.toString() + ", получен " + rhs.toString() +
                            "; неявные приведения запрещены");
        return false;
    }
    return true;
}

bool Analyzer::canCast(const Type& from, const Type& to) const {
    if (from.isError() || to.isError()) return true;
    if (from == to) return true;
    if (from.isNumeric() && to.isNumeric()) return true;
    return false;
}

bool Analyzer::isPrintable(const Type& type) const {
    return type.isNumeric() || type.kind == Type::Kind::Bool || type.kind == Type::Kind::String || type.isError();
}

bool Analyzer::isTerminatingCall(const AST::Expr& expr) const {
    const auto* call = dynamic_cast<const AST::CallExpr*>(&expr);
    if (!call) return false;
    const auto* name = dynamic_cast<const AST::NameExpr*>(call->callee.get());
    if (!name || name->path.size() != 1) return false;
    return name->path[0] == "exit" || name->path[0] == "panic";
}

std::string formatDiagnostic(const Diagnostic& diagnostic) {
    return diagnostic.file + ":" + std::to_string(diagnostic.pos.line) + ":" +
           std::to_string(diagnostic.pos.column) + ": error: " + diagnostic.message;
}

} // namespace Semantic
