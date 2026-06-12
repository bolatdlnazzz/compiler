#include "semantic.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>

namespace Semantic {
Type Type::error()   { Type t; t.kind = Kind::Error;  t.name = "<error>";  return t; }
Type Type::unit()    { Type t; t.kind = Kind::Unit;   t.name = "unit";     return t; }
Type Type::boolean() { Type t; t.kind = Kind::Bool;   t.name = "bool";     return t; }
Type Type::character() { Type t; t.kind = Kind::Char; t.name = "char"; return t; }
Type Type::string()  { Type t; t.kind = Kind::String; t.name = "string";   return t; }
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
    Type t;
    t.kind = Kind::Array;
    t.arraySize = size;
    t.elementType = std::make_shared<Type>(std::move(element));
    t.name = "[" + t.elementType->name + "; " + std::to_string(size) + "]";
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
        return arraySize == other.arraySize &&
               elementType && other.elementType &&
               *elementType == *other.elementType;
    }
    return name == other.name;
}

std::string Type::toString() const { return name; }


namespace {
AST::TypePtr cloneTypeExpr(const AST::TypeExpr& type) {
    if (const auto* named = dynamic_cast<const AST::NamedType*>(&type)) {
        auto n = std::make_unique<AST::NamedType>();
        n->path = named->path;
        n->span = named->span;
        return n;
    }
    if (const auto* arr = dynamic_cast<const AST::ArrayType*>(&type)) {
        auto a = std::make_unique<AST::ArrayType>();
        a->elementType = cloneTypeExpr(*arr->elementType);
        a->size = arr->size;
        a->span = arr->span;
        return a;
    }
    auto err = std::make_unique<AST::NamedType>();
    err->path = {"<error>"};
    err->span = type.span;
    return err;
}

AST::ExprPtr cloneExpr(const AST::Expr& expr) {
    AST::ExprPtr out;
    if (const auto* x = dynamic_cast<const AST::IntLiteralExpr*>(&expr)) {
        auto n = std::make_unique<AST::IntLiteralExpr>(); n->lexeme = x->lexeme; out = std::move(n);
    } else if (const auto* x = dynamic_cast<const AST::FloatLiteralExpr*>(&expr)) {
        auto n = std::make_unique<AST::FloatLiteralExpr>(); n->lexeme = x->lexeme; out = std::move(n);
    } else if (const auto* x = dynamic_cast<const AST::BoolLiteralExpr*>(&expr)) {
        auto n = std::make_unique<AST::BoolLiteralExpr>(); n->value = x->value; out = std::move(n);
    } else if (const auto* x = dynamic_cast<const AST::CharLiteralExpr*>(&expr)) {
        auto n = std::make_unique<AST::CharLiteralExpr>(); n->value = x->value; out = std::move(n);
    } else if (const auto* x = dynamic_cast<const AST::StringLiteralExpr*>(&expr)) {
        auto n = std::make_unique<AST::StringLiteralExpr>(); n->value = x->value; out = std::move(n);
    } else if (const auto* x = dynamic_cast<const AST::NameExpr*>(&expr)) {
        auto n = std::make_unique<AST::NameExpr>(); n->path = x->path; out = std::move(n);
    } else if (const auto* x = dynamic_cast<const AST::UnaryExpr*>(&expr)) {
        auto n = std::make_unique<AST::UnaryExpr>(); n->op = x->op; n->operand = cloneExpr(*x->operand); out = std::move(n);
    } else if (const auto* x = dynamic_cast<const AST::BinaryExpr*>(&expr)) {
        auto n = std::make_unique<AST::BinaryExpr>(); n->op = x->op; n->left = cloneExpr(*x->left); n->right = cloneExpr(*x->right); out = std::move(n);
    } else if (const auto* x = dynamic_cast<const AST::CastExpr*>(&expr)) {
        auto n = std::make_unique<AST::CastExpr>(); n->value = cloneExpr(*x->value); n->targetType = cloneTypeExpr(*x->targetType); out = std::move(n);
    } else if (const auto* x = dynamic_cast<const AST::CallExpr*>(&expr)) {
        auto n = std::make_unique<AST::CallExpr>(); n->callee = cloneExpr(*x->callee); n->argNames = x->argNames; for (auto& a : x->args) n->args.push_back(cloneExpr(*a)); out = std::move(n);
    } else if (const auto* x = dynamic_cast<const AST::FieldExpr*>(&expr)) {
        auto n = std::make_unique<AST::FieldExpr>(); n->object = cloneExpr(*x->object); n->field = x->field; out = std::move(n);
    } else if (const auto* x = dynamic_cast<const AST::IndexExpr*>(&expr)) {
        auto n = std::make_unique<AST::IndexExpr>(); n->object = cloneExpr(*x->object); n->index = cloneExpr(*x->index); out = std::move(n);
    } else if (const auto* x = dynamic_cast<const AST::ArrayLiteralExpr*>(&expr)) {
        auto n = std::make_unique<AST::ArrayLiteralExpr>(); for (auto& e : x->elements) n->elements.push_back(cloneExpr(*e)); out = std::move(n);
    } else if (const auto* x = dynamic_cast<const AST::StructLiteralExpr*>(&expr)) {
        auto n = std::make_unique<AST::StructLiteralExpr>(); n->typePath = x->typePath; for (auto& f : x->fields) { AST::StructFieldInit nf; nf.name = f.name; nf.value = cloneExpr(*f.value); nf.span = f.span; n->fields.push_back(std::move(nf)); } out = std::move(n);
    } else if (const auto* x = dynamic_cast<const AST::SizeOfExpr*>(&expr)) {
        auto n = std::make_unique<AST::SizeOfExpr>(); n->targetType = cloneTypeExpr(*x->targetType); out = std::move(n);
    } else if (const auto* x = dynamic_cast<const AST::TypeIdExpr*>(&expr)) {
        auto n = std::make_unique<AST::TypeIdExpr>(); n->target = cloneExpr(*x->target); n->fromTypeofKeyword = x->fromTypeofKeyword; out = std::move(n);
    } else if (const auto* x = dynamic_cast<const AST::IfExpr*>(&expr)) {
        auto n = std::make_unique<AST::IfExpr>(); n->condition = cloneExpr(*x->condition); n->thenValue = cloneExpr(*x->thenValue); n->elseValue = cloneExpr(*x->elseValue); out = std::move(n);
    } else {
        auto n = std::make_unique<AST::NameExpr>(); n->path = {"<error>"}; out = std::move(n);
    }
    out->span = expr.span;
    out->semanticType = expr.semanticType;
    return out;
}


AST::TypePtr typeToTypeExpr(const Type& type) {
    if (type.kind == Type::Kind::Array && type.elementType) {
        auto arr = std::make_unique<AST::ArrayType>();
        arr->elementType = typeToTypeExpr(*type.elementType);
        arr->size = type.arraySize;
        return arr;
    }
    auto n = std::make_unique<AST::NamedType>();
    if (type.name.find("::") != std::string::npos) {
        std::size_t start = 0;
        while (start < type.name.size()) {
            std::size_t pos = type.name.find("::", start);
            if (pos == std::string::npos) { n->path.push_back(type.name.substr(start)); break; }
            n->path.push_back(type.name.substr(start, pos - start));
            start = pos + 2;
        }
    } else {
        n->path.push_back(type.name);
    }
    return n;
}

AST::ExprPtr wrapImplicitCast(AST::ExprPtr value, const Type& target) {
    auto cast = std::make_unique<AST::CastExpr>();
    cast->span = value->span;
    cast->semanticType = target.name;
    cast->targetType = typeToTypeExpr(target);
    cast->value = std::move(value);
    return cast;
}
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

Analyzer::Scope* Analyzer::ensureNamespace(Scope& scope, const std::string& name, const AST::Node& node) {
    auto it = scope.symbols.find(name);
    if (it != scope.symbols.end()) {
        if (it->second.kind == SymbolKind::Namespace && it->second.namespaceScope)
            return it->second.namespaceScope;
        addDiagnostic(node, "'" + name + "' уже объявлен и не является пространством имён");
        return makeScope(&scope, true, name);
    }
    const std::string q = scope.qualifiedName.empty() ? name : scope.qualifiedName + "::" + name;
    Scope* ns = makeScope(&scope, true, q);
    Symbol sym;
    sym.kind = SymbolKind::Namespace;
    sym.name = name;
    sym.namespaceScope = ns;
    scope.symbols[name] = std::move(sym);
    return ns;
}

std::string Analyzer::qualify(std::string_view name) const {
    if (namespaceStack_.empty()) return std::string(name);
    std::string result;
    for (const auto& part : namespaceStack_) { result += part; result += "::"; }
    result += name;
    return result;
}

std::string Analyzer::joinPath(const std::vector<std::string>& path) const {
    std::string r;
    for (const auto& p : path) { if (!r.empty()) r += "::"; r += p; }
    return r;
}

std::string Analyzer::mangleFunctionName(const std::string& qualifiedName, const std::vector<Type>& params) const {
    if (qualifiedName == "main") return "main";
    std::string out = qualifiedName;
    out += "__";
    if (params.empty()) out += "void";
    for (std::size_t i = 0; i < params.size(); ++i) {
        if (i) out += "_";
        for (char c : params[i].toString()) {
            if (std::isalnum(static_cast<unsigned char>(c))) out += c;
            else out += '_';
        }
    }
    return out;
}

Analyzer::Symbol* Analyzer::lookupMethod(const std::string& receiverType, const std::string& methodName) {
    std::vector<std::string> candidates;
    candidates.push_back(receiverType + "::" + methodName);
    const std::size_t pos = receiverType.rfind("::");
    if (pos != std::string::npos) candidates.push_back(receiverType.substr(pos + 2) + "::" + methodName);
    for (const auto& candidate : candidates) {
        if (Symbol* s = lookupLexical(candidate)) return s;
        if (rootScope_) {
            auto it = rootScope_->symbols.find(candidate);
            if (it != rootScope_->symbols.end()) return &it->second;
        }
        if (moduleScope_) {
            auto it = moduleScope_->symbols.find(candidate);
            if (it != moduleScope_->symbols.end()) return &it->second;
        }
    }
    return nullptr;
}

void Analyzer::addDiagnostic(const AST::Node& node, std::string message) {
    diagnostics_.push_back(Diagnostic{fileName_, node.span.begin, std::move(message)});
}

void Analyzer::addDiagnostic(Lexer::Position pos, std::string message) {
    diagnostics_.push_back(Diagnostic{fileName_, pos, std::move(message)});
}

void Analyzer::addBuiltinFunction(std::string name, Type returnType, std::vector<Type> params) {
    auto info = std::make_shared<FunctionInfo>();
    info->name = name;
    info->qualifiedName = name;
    info->returnType = std::move(returnType);
    info->paramTypes = std::move(params);
    info->isBuiltin = true;
    info->builtinName = info->name;
    Symbol sym;
    sym.kind = SymbolKind::Function;
    sym.name = name;
    sym.type = info->returnType;
    sym.function = info;
    rootScope_->symbols[name] = std::move(sym);
}

void Analyzer::installBuiltins() {
    addBuiltinFunction("print", Type::unit());
    addBuiltinFunction("input", Type::string());
    addBuiltinFunction("exit",  Type::unit(), {Type::integer("int32", 32, false)});
    addBuiltinFunction("panic", Type::unit(), {Type::string()});
    addBuiltinFunction("assert", Type::unit(), {Type::boolean()});
    addBuiltinFunction("len",   Type::integer("int32", 32, false));
}

bool Analyzer::declare(Scope& scope, const Symbol& symbol, const AST::Node& node) {
    auto it = scope.symbols.find(symbol.name);
    if (it != scope.symbols.end()) {
        if (it->second.kind == SymbolKind::Function && symbol.kind == SymbolKind::Function && symbol.function) {
            auto& overloads = it->second.overloads;
            if (overloads.empty() && it->second.function) overloads.push_back(it->second.function);
            for (const auto& existing : overloads) {
                if (existing && existing->paramTypes.size() == symbol.function->paramTypes.size()) {
                    bool sameSignature = true;
                    for (std::size_t i = 0; i < existing->paramTypes.size(); ++i) {
                        if (existing->paramTypes[i] != symbol.function->paramTypes[i]) {
                            sameSignature = false;
                            break;
                        }
                    }
                    if (sameSignature) {
                        addDiagnostic(node, "перегрузка функции '" + symbol.name + "' с такой сигнатурой уже объявлена");
                        return false;
                    }
                }
            }
            overloads.push_back(symbol.function);
            return true;
        }
        addDiagnostic(node, "'" + symbol.name + "' уже объявлен в этой области видимости");
        return false;
    }
    scope.symbols[symbol.name] = symbol;
    if (symbol.kind == SymbolKind::Function && symbol.function) {
        scope.symbols[symbol.name].overloads.push_back(symbol.function);
    }
    return true;
}

Analyzer::Symbol* Analyzer::lookupLocal(Scope& scope, const std::string& name) {
    auto it = scope.symbols.find(name);
    if (it != scope.symbols.end()) return &it->second;
    return nullptr;
}

Analyzer::Symbol* Analyzer::lookupLexical(const std::string& name) {
    Scope* s = currentScope_;
    while (s) {
        auto it = s->symbols.find(name);
        if (it != s->symbols.end()) return &it->second;
        s = s->parent;
    }
    return nullptr;
}

Analyzer::Symbol* Analyzer::resolvePathFromScope(Scope& start, const std::vector<std::string>& path, const AST::Node& ) {
    Scope* s = &start;
    for (std::size_t i = 0; i < path.size(); ++i) {
        auto it = s->symbols.find(path[i]);
        if (it == s->symbols.end()) return nullptr;
        if (i + 1 == path.size()) return &it->second;
        if (it->second.kind == SymbolKind::Namespace && it->second.namespaceScope)
            s = it->second.namespaceScope;
        else return nullptr;
    }
    return nullptr;
}

Analyzer::Symbol* Analyzer::resolvePath(const std::vector<std::string>& path, const AST::Node& node) {
    if (path.empty()) return nullptr;
    if (path.size() == 1) return lookupLexical(path[0]);
    Symbol* s = resolvePathFromScope(*rootScope_, path, node);
    if (s) return s;
    if (moduleScope_) {
        s = resolvePathFromScope(*moduleScope_, path, node);
        if (s) return s;
    }
    return nullptr;
}

bool Analyzer::analyze(AST::Module& module) {
    diagnostics_.clear();
    ownedScopes_.clear();
    rootScope_ = makeScope(nullptr, false, "");
    installBuiltins();
    if (!module.namePath.empty()) {
        Scope* s = rootScope_;
        for (const auto& part : module.namePath) {
            std::string q = s->qualifiedName.empty() ? part : s->qualifiedName + "::" + part;
            s = ensureNamespace(*s, part, module);
        }
        moduleScope_ = s;
    } else {
        moduleScope_ = rootScope_;
    }
    currentScope_ = moduleScope_;
    for (auto& decl : module.decls) analyzeDecl(*decl);
    Symbol* mainSym = lookupLexical("main");
    if (!mainSym || mainSym->kind != SymbolKind::Function || mainSym->overloads.empty()) {
        addDiagnostic(module, "программа должна содержать функцию main");
    } else {
        bool okMain = false;
        for (const auto& fn : mainSym->overloads) {
            if (!fn || !fn->decl) continue;
            const Type& ret = fn->returnType;
            if (fn->paramTypes.empty() && ret.kind == Type::Kind::Int &&
                (ret.name == "int32" || ret.name == "int64" || ret.name == "int")) {
                okMain = true;
                break;
            }
        }
        if (!okMain) addDiagnostic(*mainSym->overloads.front()->decl, "функция main должна иметь сигнатуру main() -> int32/int64");
    }
    return diagnostics_.empty();
}

void Analyzer::analyzeDecl(AST::Decl& decl) {
    if (auto* ns = dynamic_cast<AST::NamespaceDecl*>(&decl))      return analyzeNamespaceDecl(*ns);
    if (auto* ta = dynamic_cast<AST::TypeAliasDecl*>(&decl))      return analyzeTypeAliasDecl(*ta);
    if (auto* st = dynamic_cast<AST::StructDecl*>(&decl))         return analyzeStructDecl(*st);
    if (auto* fn = dynamic_cast<AST::FunctionDecl*>(&decl))       return analyzeFunctionDecl(*fn);
}

void Analyzer::analyzeNamespaceDecl(AST::NamespaceDecl& decl) {
    Scope* ns = ensureNamespace(*currentScope_, decl.name, decl);
    Scope* saved = currentScope_;
    currentScope_ = ns;
    namespaceStack_.push_back(decl.name);
    for (auto& child : decl.decls) analyzeDecl(*child);
    namespaceStack_.pop_back();
    currentScope_ = saved;
}

void Analyzer::analyzeTypeAliasDecl(AST::TypeAliasDecl& decl) {
    Type aliased = resolveType(*decl.aliasedType);
    Symbol sym;
    sym.kind = SymbolKind::Alias;
    sym.name = decl.name;
    sym.type = aliased;
    declare(*currentScope_, sym, decl);
}

void Analyzer::analyzeStructDecl(AST::StructDecl& decl) {
    const std::string qname = qualify(decl.name);
    auto info = std::make_shared<StructInfo>();
    info->name = decl.name;
    info->qualifiedName = qname;
    for (auto& f : decl.fields) {
        Type ft = resolveType(*f.type);
        info->fieldsInOrder.push_back({f.name, ft});
        info->fields[f.name] = ft;
    }
    Symbol sym;
    sym.kind = SymbolKind::Struct;
    sym.name = decl.name;
    sym.type = Type::structure(qname);
    sym.structure = info;
    declare(*currentScope_, sym, decl);
}

void Analyzer::analyzeFunctionDecl(AST::FunctionDecl& decl) {
    const std::string sourceName = decl.methodOf.empty()
        ? decl.name
        : joinPath(decl.methodOf) + "::" + decl.name;
    const std::string qname = qualify(sourceName);
    auto info = std::make_shared<FunctionInfo>();
    info->name = sourceName;
    info->qualifiedName = qname;
    info->decl = &decl;

    bool sawDefault = false;
    for (auto& p : decl.params) {
        Type pt = resolveType(*p.type);
        info->paramNames.push_back(p.name);
        info->paramTypes.push_back(pt);
        info->defaultArgs.push_back(p.defaultValue.get());
        if (p.defaultValue) {
            sawDefault = true;
            Type dt = analyzeExpr(*p.defaultValue, pt);
            if (!dt.isError() && !pt.isError()) checkAssignable(pt, dt, *p.defaultValue);
        } else if (sawDefault) {
            addDiagnostic(decl, "параметр без значения по умолчанию не может идти после параметра со значением по умолчанию");
        }
    }

    if (!decl.methodOf.empty()) {
        if (decl.params.empty()) {
            addDiagnostic(decl, "метод структуры должен иметь первый параметр self");
        } else {
            const std::string receiverName = joinPath(decl.methodOf);
            Type receiver = resolveType(*decl.params[0].type);
            if (!receiver.isError() && receiver.name != receiverName && receiver.name != qualify(receiverName)) {
                addDiagnostic(decl, "первый параметр метода должен иметь тип " + receiverName);
            }
        }
    }

    if (decl.returnType) info->returnType = resolveType(*decl.returnType);
    else info->returnType = Type::error();
    info->codegenName = mangleFunctionName(qname, info->paramTypes);
    if (qname == "main") info->codegenName = "main";
    decl.qualifiedNameForCodegen = info->codegenName;

    Symbol sym;
    sym.kind = SymbolKind::Function;
    sym.name = sourceName;
    sym.type = info->returnType;
    sym.function = info;
    declare(*currentScope_, sym, decl);

    Scope* saved = currentScope_;
    Scope* fnScope = makeScope(currentScope_, false);
    currentScope_ = fnScope;
    Type savedReturn = currentReturnType_;
    bool savedInfers = currentFunctionInfersReturn_;
    bool savedSawReturn = currentFunctionSawReturnValue_;
    currentReturnType_ = info->returnType;
    currentFunctionInfersReturn_ = !decl.returnType;
    currentFunctionSawReturnValue_ = false;

    for (std::size_t i = 0; i < decl.params.size(); ++i) {
        Symbol paramSym;
        paramSym.kind = SymbolKind::Variable;
        paramSym.name = decl.params[i].name;
        paramSym.type = info->paramTypes[i];
        paramSym.isMutable = false;
        declare(*currentScope_, paramSym, decl);
    }
    analyzeBlock(*decl.body, false);
    if (!decl.returnType) {
        info->returnType = currentFunctionSawReturnValue_ ? currentReturnType_ : Type::unit();
    }
    currentReturnType_ = savedReturn;
    currentFunctionInfersReturn_ = savedInfers;
    currentFunctionSawReturnValue_ = savedSawReturn;
    currentScope_ = saved;
}

Type Analyzer::resolveType(AST::TypeExpr& typeExpr) {
    if (auto* named = dynamic_cast<AST::NamedType*>(&typeExpr)) return resolveNamedType(*named);
    if (auto* arr = dynamic_cast<AST::ArrayType*>(&typeExpr)) {
        Type elem = resolveType(*arr->elementType);
        return Type::array(elem, arr->size);
    }
    addDiagnostic(typeExpr, "неизвестный вид типа");
    return Type::error();
}

Type Analyzer::resolveNamedType(AST::NamedType& typeExpr) {
    const std::string name = joinPath(typeExpr.path);
    if (name == "int8")    return Type::integer("int8",  8,  false);
    if (name == "int16")   return Type::integer("int16", 16, false);
    if (name == "int32")   return Type::integer("int32", 32, false);
    if (name == "int64")   return Type::integer("int64", 64, false);
    if (name == "uint8")   return Type::integer("uint8",  8,  true);
    if (name == "uint16")  return Type::integer("uint16", 16, true);
    if (name == "uint32")  return Type::integer("uint32", 32, true);
    if (name == "uint64")  return Type::integer("uint64", 64, true);
    if (name == "float32") return Type::floating("float32", 32);
    if (name == "float64") return Type::floating("float64", 64);
    if (name == "bool")    return Type::boolean();
    if (name == "char")    return Type::character();
    if (name == "string")  return Type::string();
    if (name == "unit")    return Type::unit();
    Symbol* sym = resolvePath(typeExpr.path, typeExpr);
    if (!sym) {
        addDiagnostic(typeExpr, "неизвестный тип '" + name + "'");
        return Type::error();
    }
    if (sym->kind == SymbolKind::Alias) return sym->type;
    if (sym->kind == SymbolKind::Struct) return sym->type;
    addDiagnostic(typeExpr, "'" + name + "' не является типом");
    return Type::error();
}

Analyzer::Flow Analyzer::analyzeBlock(AST::BlockStmt& block, bool createScope) {
    Scope* saved = currentScope_;
    if (createScope) currentScope_ = makeScope(currentScope_, false);
    Flow flow = Flow::MayContinue;
    for (auto& stmt : block.statements) {
        Flow f = analyzeStmt(*stmt);
        if (f == Flow::NoContinue) flow = Flow::NoContinue;
    }
    if (createScope) currentScope_ = saved;
    return flow;
}

Analyzer::Flow Analyzer::analyzeStmt(AST::Stmt& stmt) {
    if (dynamic_cast<AST::EmptyStmt*>(&stmt)) return Flow::MayContinue;
    if (auto* b = dynamic_cast<AST::BlockStmt*>(&stmt)) return analyzeBlock(*b, true);
    if (auto* let = dynamic_cast<AST::LetStmt*>(&stmt)) {
        std::optional<Type> expected;
        if (let->explicitType) expected = resolveType(*let->explicitType);
        Type init = analyzeExpr(*let->initializer, expected);
        if (expected && !expected->isError() && !init.isError())
            checkAssignable(*expected, init, *let->initializer);
        Type varType = expected.value_or(init);
        if (varType.isError()) varType = init;
        Symbol sym;
        sym.kind = SymbolKind::Variable;
        sym.name = let->name;
        sym.type = varType;
        sym.isMutable = false;
        declare(*currentScope_, sym, stmt);
        return Flow::MayContinue;
    }
    if (auto* var = dynamic_cast<AST::VarStmt*>(&stmt)) {
        Type declared = resolveType(*var->explicitType);
        Type init = analyzeExpr(*var->initializer, declared);
        if (!declared.isError() && !init.isError())
            checkAssignable(declared, init, *var->initializer);
        Symbol sym;
        sym.kind = SymbolKind::Variable;
        sym.name = var->name;
        sym.type = declared;
        sym.isMutable = true;
        declare(*currentScope_, sym, stmt);
        return Flow::MayContinue;
    }
    if (auto* asg = dynamic_cast<AST::AssignStmt*>(&stmt)) {
        LValueInfo lv = analyzeLValue(*asg->target);
        if (!lv.isLValue)
            addDiagnostic(*asg->target, "левая часть присваивания не является l-value");
        else if (!lv.isMutable)
            addDiagnostic(*asg->target, "нельзя присваивать иммутабельной переменной");
        Type rhs = analyzeExpr(*asg->value, lv.isLValue ? std::optional<Type>(lv.type) : std::nullopt);
        if (lv.isLValue && !lv.type.isError() && !rhs.isError())
            checkAssignable(lv.type, rhs, *asg->value);
        return Flow::MayContinue;
    }
    if (auto* es = dynamic_cast<AST::ExprStmt*>(&stmt)) return analyzeExprStmt(*es);
    if (auto* ifs = dynamic_cast<AST::IfStmt*>(&stmt)) return analyzeIf(*ifs);
    if (auto* wh  = dynamic_cast<AST::WhileStmt*>(&stmt)) return analyzeWhile(*wh);
    if (dynamic_cast<AST::BreakStmt*>(&stmt)) {
        if (loopDepth_ == 0) addDiagnostic(stmt, "break вне цикла");
        return Flow::MayContinue;
    }
    if (dynamic_cast<AST::ContinueStmt*>(&stmt)) {
        if (loopDepth_ == 0) addDiagnostic(stmt, "continue вне цикла");
        return Flow::MayContinue;
    }
    if (auto* ret = dynamic_cast<AST::ReturnStmt*>(&stmt)) {
        if (ret->value) {
            if (currentFunctionInfersReturn_) {
                Type vt = analyzeExpr(*ret->value);
                if (!vt.isError()) {
                    if (!currentFunctionSawReturnValue_) {
                        currentReturnType_ = vt;
                        currentFunctionSawReturnValue_ = true;
                    } else {
                        checkAssignable(currentReturnType_, vt, *ret->value);
                    }
                }
            } else {
                Type vt = analyzeExpr(*ret->value, currentReturnType_);
                if (!currentReturnType_.isError() && !vt.isError())
                    checkAssignable(currentReturnType_, vt, *ret->value);
            }
        } else {
            if (currentFunctionInfersReturn_) {
                if (!currentFunctionSawReturnValue_) currentReturnType_ = Type::unit();
            } else if (currentReturnType_.kind != Type::Kind::Unit) {
                addDiagnostic(stmt, "функция должна вернуть значение типа " + currentReturnType_.toString());
            }
        }
        return Flow::NoContinue;
    }
    addDiagnostic(stmt, "неизвестный оператор");
    return Flow::MayContinue;
}

Analyzer::Flow Analyzer::analyzeIf(AST::IfStmt& stmt) {
    Type cond = analyzeExpr(*stmt.condition);
    if (!cond.isError() && cond.kind != Type::Kind::Bool)
        addDiagnostic(*stmt.condition, "условие if должно быть bool, получен " + cond.toString());
    Flow thenFlow = analyzeBlock(*stmt.thenBlock, true);
    if (stmt.elseBranch) {
        Flow elseFlow = analyzeStmt(*stmt.elseBranch);
        if (thenFlow == Flow::NoContinue && elseFlow == Flow::NoContinue)
            return Flow::NoContinue;
    }
    return Flow::MayContinue;
}

Analyzer::Flow Analyzer::analyzeWhile(AST::WhileStmt& stmt) {
    Type cond = analyzeExpr(*stmt.condition);
    if (!cond.isError() && cond.kind != Type::Kind::Bool)
        addDiagnostic(*stmt.condition, "условие while должно быть bool, получен " + cond.toString());
    ++loopDepth_;
    analyzeBlock(*stmt.body, true);
    --loopDepth_;
    return Flow::MayContinue;
}

Analyzer::Flow Analyzer::analyzeExprStmt(AST::ExprStmt& stmt) {
    analyzeExpr(*stmt.expr);
    return Flow::MayContinue;
}

static void setType(AST::Expr& expr, const Type& t) {
    expr.semanticType = t.name;
}

Type Analyzer::analyzeExpr(AST::Expr& expr, const std::optional<Type>& expected) {
    Type result = Type::error();
    if (dynamic_cast<AST::IntLiteralExpr*>(&expr)) {
        if (expected && (expected->kind == Type::Kind::Int || expected->kind == Type::Kind::UInt))
            result = *expected;
        else
            result = Type::integer("int32", 32, false);
    }
    else if (dynamic_cast<AST::FloatLiteralExpr*>(&expr)) {
        if (expected && expected->kind == Type::Kind::Float) result = *expected;
        else result = Type::floating("float64", 64);
    }
    else if (dynamic_cast<AST::BoolLiteralExpr*>(&expr)) {
        result = Type::boolean();
    }
    else if (dynamic_cast<AST::CharLiteralExpr*>(&expr)) {
        result = Type::character();
    }
    else if (dynamic_cast<AST::StringLiteralExpr*>(&expr)) {
        result = Type::string();
    }
    else if (auto* e = dynamic_cast<AST::NameExpr*>(&expr)) {
        result = analyzeNameExpr(*e);
    }
    else if (auto* e = dynamic_cast<AST::ArrayLiteralExpr*>(&expr)) {
        result = analyzeArrayLiteral(*e, expected);
    }
    else if (auto* e = dynamic_cast<AST::StructLiteralExpr*>(&expr)) {
        result = analyzeStructLiteral(*e);
    }
    else if (auto* e = dynamic_cast<AST::UnaryExpr*>(&expr)) {
        result = analyzeUnary(*e);
    }
    else if (auto* e = dynamic_cast<AST::BinaryExpr*>(&expr)) {
        result = analyzeBinary(*e);
    }
    else if (auto* e = dynamic_cast<AST::CastExpr*>(&expr)) {
        result = analyzeCast(*e);
    }
    else if (auto* e = dynamic_cast<AST::SizeOfExpr*>(&expr)) {
        result = analyzeSizeOf(*e);
    }
    else if (auto* e = dynamic_cast<AST::TypeIdExpr*>(&expr)) {
        result = analyzeTypeId(*e);
    }
    else if (auto* e = dynamic_cast<AST::IfExpr*>(&expr)) {
        result = analyzeIfExpr(*e, expected);
    }
    else if (auto* e = dynamic_cast<AST::CallExpr*>(&expr)) {
        result = analyzeCall(*e);
    }
    else if (auto* e = dynamic_cast<AST::FieldExpr*>(&expr)) {
        result = analyzeField(*e);
    }
    else if (auto* e = dynamic_cast<AST::IndexExpr*>(&expr)) {
        result = analyzeIndex(*e);
    }
    else {
        addDiagnostic(expr, "неизвестное выражение");
    }
    setType(expr, result);
    return result;
}

Type Analyzer::analyzeNameExpr(AST::NameExpr& expr) {
    Symbol* sym = resolvePath(expr.path, expr);
    if (!sym) {
        addDiagnostic(expr, "неизвестное имя '" + joinPath(expr.path) + "'");
        return Type::error();
    }
    if (sym->kind == SymbolKind::Variable) return sym->type;
    if (sym->kind == SymbolKind::Function) return sym->type;
    addDiagnostic(expr, "'" + joinPath(expr.path) + "' не является значением");
    return Type::error();
}

Type Analyzer::analyzeArrayLiteral(AST::ArrayLiteralExpr& expr, const std::optional<Type>& expected) {
    if (expr.elements.empty()) {
        addDiagnostic(expr, "литерал пустого массива требует явной аннотации типа");
        return Type::error();
    }
    std::optional<Type> elemExpected;
    if (expected && expected->kind == Type::Kind::Array && expected->elementType)
        elemExpected = *expected->elementType;
    Type firstElem = analyzeExpr(*expr.elements[0], elemExpected);
    for (std::size_t i = 1; i < expr.elements.size(); ++i) {
        Type t = analyzeExpr(*expr.elements[i], firstElem);
        if (!t.isError() && !firstElem.isError() && t != firstElem)
            addDiagnostic(*expr.elements[i], "элементы массива имеют разные типы: " +
                          firstElem.toString() + " vs " + t.toString());
    }
    return Type::array(firstElem, expr.elements.size());
}

Type Analyzer::analyzeStructLiteral(AST::StructLiteralExpr& expr) {
    Symbol* sym = resolvePath(expr.typePath, expr);
    if (!sym || sym->kind != SymbolKind::Struct || !sym->structure) {
        addDiagnostic(expr, "неизвестный тип структуры '" + joinPath(expr.typePath) + "'");
        for (auto& f : expr.fields) analyzeExpr(*f.value);
        return Type::error();
    }
    const auto& info = *sym->structure;
    for (auto& f : expr.fields) {
        auto it = info.fields.find(f.name);
        if (it == info.fields.end()) {
            addDiagnostic(expr, "поле '" + f.name + "' не существует в структуре " + info.qualifiedName);
            analyzeExpr(*f.value);
        } else {
            Type vt = analyzeExpr(*f.value, it->second);
            if (!vt.isError() && !it->second.isError())
                checkAssignable(it->second, vt, *f.value);
        }
    }
    return Type::structure(info.qualifiedName);
}

Type Analyzer::analyzeUnary(AST::UnaryExpr& expr) {
    Type operand = analyzeExpr(*expr.operand);
    if (operand.isError()) return Type::error();
    if (expr.op == "-") {
        if (!operand.isNumeric()) {
            addDiagnostic(expr, "унарный минус применим только к числовым типам, получен " + operand.toString());
            return Type::error();
        }
        return operand;
    }
    if (expr.op == "!") {
        if (operand.kind != Type::Kind::Bool) {
            addDiagnostic(expr, "логическое отрицание применимо только к bool, получен " + operand.toString());
            return Type::error();
        }
        return Type::boolean();
    }
    if (expr.op == "~") {
        if (!operand.isInteger()) {
            addDiagnostic(expr, "битовое отрицание применимо только к целым типам, получен " + operand.toString());
            return Type::error();
        }
        return operand;
    }
    addDiagnostic(expr, "неизвестный унарный оператор '" + expr.op + "'");
    return Type::error();
}

Type Analyzer::analyzeBinary(AST::BinaryExpr& expr) {
    Type left  = analyzeExpr(*expr.left);
    Type right = analyzeExpr(*expr.right);
    if (left.isError() || right.isError()) return Type::error();
    const auto& op = expr.op;
    if (op == "&&" || op == "||") {
        if (left.kind != Type::Kind::Bool)
            addDiagnostic(*expr.left,  "оператор " + op + " требует bool, получен " + left.toString());
        if (right.kind != Type::Kind::Bool)
            addDiagnostic(*expr.right, "оператор " + op + " требует bool, получен " + right.toString());
        return Type::boolean();
    }
    if (op == "+" && left.kind == Type::Kind::String) {
        if (right.kind != Type::Kind::String)
            addDiagnostic(expr, "оператор + для строк требует string справа, получен " + right.toString());
        return Type::string();
    }
    if ((op == "==" || op == "!=") && left.kind == Type::Kind::String) {
        if (right.kind != Type::Kind::String)
            addDiagnostic(expr, "нельзя сравнивать string с " + right.toString());
        return Type::boolean();
    }
    if ((op == "==" || op == "!=") && (left.kind == Type::Kind::Array || left.kind == Type::Kind::Struct)) {
        if (left != right)
            addDiagnostic(expr, "нельзя сравнивать " + left.toString() + " с " + right.toString());
        return Type::boolean();
    }
    if (op == "&" || op == "|" || op == "^" || op == "<<" || op == ">>") {
        if (!left.isInteger() || !right.isInteger()) {
            addDiagnostic(expr, "битовый оператор " + op + " требует целочисленные операнды, получены " + left.toString() + " и " + right.toString());
            return Type::error();
        }
        if (op != "<<" && op != ">>" && left != right) {
            addDiagnostic(expr, "битовый оператор " + op + " требует одинаковые типы, получены " + left.toString() + " и " + right.toString());
        }
        return left;
    }
    if (left.isNumeric() && right.isNumeric()) {
        if (left != right)
            addDiagnostic(expr, "несовместимые типы для оператора " + op + ": " +
                          left.toString() + " и " + right.toString());
        if (op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=")
            return Type::boolean();
        if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%")
            return left;
        addDiagnostic(expr, "оператор " + op + " не поддерживается для числовых типов");
        return Type::error();
    }
    if ((op == "==" || op == "!=") && left.kind == Type::Kind::Bool && right.kind == Type::Kind::Bool)
        return Type::boolean();
    if ((op == "==" || op == "!=") && left.kind == Type::Kind::Char && right.kind == Type::Kind::Char)
        return Type::boolean();
    addDiagnostic(expr, "оператор " + op + " не применим к типам " + left.toString() + " и " + right.toString());
    return Type::error();
}

Type Analyzer::analyzeCast(AST::CastExpr& expr) {
    Type from = analyzeExpr(*expr.value);
    Type to   = resolveType(*expr.targetType);
    if (from.isError() || to.isError()) return to.isError() ? Type::error() : to;
    if (!canCast(from, to)) {
        addDiagnostic(expr, "недопустимое приведение из " + from.toString() + " в " + to.toString());
        return to;
    }
    return to;
}

Type Analyzer::analyzeSizeOf(AST::SizeOfExpr& expr) {
    Type t = resolveType(*expr.targetType);
    (void)t;
    return Type::integer("int32", 32, false);
}

Type Analyzer::analyzeTypeId(AST::TypeIdExpr& expr) {
    Type t = analyzeExpr(*expr.target);
    (void)t;
    return Type::string();
}


Type Analyzer::analyzeIfExpr(AST::IfExpr& expr, const std::optional<Type>& expected) {
    Type cond = analyzeExpr(*expr.condition, Type::boolean());
    if (!cond.isError() && cond.kind != Type::Kind::Bool) {
        addDiagnostic(*expr.condition, "условие if-выражения должно иметь тип bool");
    }
    Type thenType = analyzeExpr(*expr.thenValue, expected);
    Type elseType = analyzeExpr(*expr.elseValue, expected ? expected : std::optional<Type>(thenType));
    if (thenType.isError() || elseType.isError()) return Type::error();
    if (thenType == elseType) return thenType;
    if (canAssignSilently(thenType, elseType)) {
        expr.elseValue = wrapImplicitCast(std::move(expr.elseValue), thenType);
        return thenType;
    }
    if (canAssignSilently(elseType, thenType)) {
        expr.thenValue = wrapImplicitCast(std::move(expr.thenValue), elseType);
        return elseType;
    }
    addDiagnostic(expr, "ветки if-выражения имеют несовместимые типы: " + thenType.toString() + " и " + elseType.toString());
    return Type::error();
}

Type Analyzer::analyzeCall(AST::CallExpr& expr) {
    auto analyzeBuiltin = [&](const std::string& name) -> std::optional<Type> {
        if (name == "print") {
            if (expr.args.size() != 1) {
                addDiagnostic(expr, "print ожидает 1 аргумент");
            } else {
                Type argType = analyzeExpr(*expr.args[0]);
                if (!argType.isError() && !isPrintable(argType))
                    addDiagnostic(*expr.args[0], "тип " + argType.toString() + " не поддерживается функцией print");
            }
            setType(*expr.callee, Type::unit());
            return Type::unit();
        }
        if (name == "assert") {
            if (expr.args.size() != 1) {
                addDiagnostic(expr, "assert ожидает 1 аргумент");
            } else {
                Type argType = analyzeExpr(*expr.args[0], Type::boolean());
                if (!argType.isError() && argType.kind != Type::Kind::Bool)
                    addDiagnostic(*expr.args[0], "assert ожидает bool, получен " + argType.toString());
            }
            setType(*expr.callee, Type::unit());
            return Type::unit();
        }
        if (name == "len") {
            if (expr.args.size() != 1) {
                addDiagnostic(expr, "len ожидает 1 аргумент");
            } else {
                Type argType = analyzeExpr(*expr.args[0]);
                if (!argType.isError() && argType.kind != Type::Kind::String && argType.kind != Type::Kind::Array)
                    addDiagnostic(*expr.args[0], "len ожидает string или массив, получен " + argType.toString());
            }
            setType(*expr.callee, Type::integer("int32", 32, false));
            return Type::integer("int32", 32, false);
        }
        if (name == "input") {
            if (!expr.args.empty()) addDiagnostic(expr, "input не принимает аргументы");
            return Type::string();
        }
        if (name == "exit") {
            if (expr.args.size() != 1) addDiagnostic(expr, "exit ожидает 1 аргумент");
            else analyzeExpr(*expr.args[0], Type::integer("int32", 32, false));
            return Type::unit();
        }
        if (name == "panic") {
            if (expr.args.size() != 1) addDiagnostic(expr, "panic ожидает 1 аргумент");
            else analyzeExpr(*expr.args[0], Type::string());
            return Type::unit();
        }
        return std::nullopt;
    };

    Symbol* sym = nullptr;
    std::string displayName;

    if (auto* calleeName = dynamic_cast<AST::NameExpr*>(expr.callee.get())) {
        displayName = joinPath(calleeName->path);
        if (calleeName->path.size() == 1) {
            if (auto builtin = analyzeBuiltin(displayName)) return *builtin;
        }
        sym = resolvePath(calleeName->path, expr);
    } else if (auto* method = dynamic_cast<AST::FieldExpr*>(expr.callee.get())) {
        Type receiverType = analyzeExpr(*method->object);
        displayName = receiverType.toString() + "." + method->field;
        if (!receiverType.isError() && receiverType.kind == Type::Kind::Struct) {
            sym = lookupMethod(receiverType.name, method->field);
            if (sym && sym->kind == SymbolKind::Function) {
                expr.args.insert(expr.args.begin(), std::move(method->object));
                expr.argNames.insert(expr.argNames.begin(), std::nullopt);
            }
        }
    } else {
        addDiagnostic(expr, "вызов только по имени функции или метода структуры");
        for (auto& a : expr.args) analyzeExpr(*a);
        return Type::error();
    }

    if (!sym || sym->kind != SymbolKind::Function || sym->overloads.empty()) {
        addDiagnostic(expr, "'" + displayName + "' не является функцией");
        for (auto& a : expr.args) if (a) analyzeExpr(*a);
        return Type::error();
    }

    std::vector<Type> providedTypes;
    providedTypes.reserve(expr.args.size());
    for (auto& a : expr.args) providedTypes.push_back(analyzeExpr(*a));

    auto candidateScore = [&](const FunctionInfo& fn) -> std::optional<int> {
        std::vector<bool> filled(fn.paramTypes.size(), false);
        std::size_t nextPos = 0;
        int score = 0;
        for (std::size_t i = 0; i < expr.args.size(); ++i) {
            int target = -1;
            if (i < expr.argNames.size() && expr.argNames[i]) {
                for (std::size_t p = 0; p < fn.paramNames.size(); ++p) {
                    if (fn.paramNames[p] == *expr.argNames[i]) { target = static_cast<int>(p); break; }
                }
                if (target < 0) return std::nullopt;
            } else {
                while (nextPos < filled.size() && filled[nextPos]) ++nextPos;
                if (nextPos >= fn.paramTypes.size()) return std::nullopt;
                target = static_cast<int>(nextPos++);
            }
            if (filled[target]) return std::nullopt;
            filled[target] = true;
            if (!providedTypes[i].isError()) {
                int s = implicitConversionScore(fn.paramTypes[target], providedTypes[i]);
                if (s < 0) return std::nullopt;
                score += s;
            }
        }
        for (std::size_t p = 0; p < filled.size(); ++p) {
            if (!filled[p] && (p >= fn.defaultArgs.size() || fn.defaultArgs[p] == nullptr)) return std::nullopt;
        }
        return score;
    };

    std::shared_ptr<FunctionInfo> selected;
    std::optional<int> bestScore;
    bool ambiguous = false;
    for (auto& fn : sym->overloads) {
        if (!fn) continue;
        auto score = candidateScore(*fn);
        if (!score) continue;
        if (!selected || *score < *bestScore) {
            selected = fn;
            bestScore = score;
            ambiguous = false;
        } else if (*score == *bestScore) {
            ambiguous = true;
        }
    }
    if (ambiguous) {
        addDiagnostic(expr, "неоднозначный вызов перегруженной функции '" + displayName + "'");
        return Type::error();
    }
    if (!selected) {
        addDiagnostic(expr, "нет подходящей перегрузки функции '" + displayName + "' для переданных аргументов");
        return Type::error();
    }

    // Normalize named/default arguments into plain positional arguments for codegen.
    std::vector<AST::ExprPtr> normalized(selected->paramTypes.size());
    std::size_t nextPos = 0;
    for (std::size_t i = 0; i < expr.args.size(); ++i) {
        std::size_t target = 0;
        if (i < expr.argNames.size() && expr.argNames[i]) {
            auto it = std::find(selected->paramNames.begin(), selected->paramNames.end(), *expr.argNames[i]);
            target = static_cast<std::size_t>(std::distance(selected->paramNames.begin(), it));
        } else {
            while (nextPos < normalized.size() && normalized[nextPos]) ++nextPos;
            target = nextPos++;
        }
        if (target < normalized.size()) normalized[target] = std::move(expr.args[i]);
    }
    for (std::size_t i = 0; i < normalized.size(); ++i) {
        if (!normalized[i] && i < selected->defaultArgs.size() && selected->defaultArgs[i]) {
            normalized[i] = cloneExpr(*selected->defaultArgs[i]);
            Type dt = analyzeExpr(*normalized[i], selected->paramTypes[i]);
            if (!dt.isError()) checkAssignable(selected->paramTypes[i], dt, *normalized[i]);
        }
    }
    expr.args = std::move(normalized);
    expr.argNames.clear();

    for (std::size_t i = 0; i < expr.args.size(); ++i) {
        Type at = analyzeExpr(*expr.args[i], selected->paramTypes[i]);
        if (!at.isError() && checkAssignable(selected->paramTypes[i], at, *expr.args[i]) && at != selected->paramTypes[i]) {
            expr.args[i] = wrapImplicitCast(std::move(expr.args[i]), selected->paramTypes[i]);
        }
    }

    expr.resolvedCalleeName = selected->codegenName.empty() ? selected->qualifiedName : selected->codegenName;
    if (expr.callee) setType(*expr.callee, selected->returnType);
    return selected->returnType;
}

Type Analyzer::analyzeField(AST::FieldExpr& expr) {
    Type objType = analyzeExpr(*expr.object);
    if (objType.isError()) return Type::error();
    if (objType.kind != Type::Kind::Struct) {
        addDiagnostic(expr, "доступ к полю применим только к структурам, получен " + objType.toString());
        return Type::error();
    }
    Symbol* sym = lookupLexical(objType.name);
    if (!sym) {
        for (auto& [k, v] : rootScope_->symbols) {
            if (v.kind == SymbolKind::Struct && v.structure && v.structure->qualifiedName == objType.name) {
                sym = &v;
                break;
            }
        }
    }
    if (!sym || !sym->structure) {
        addDiagnostic(expr, "не найдена структура " + objType.toString());
        return Type::error();
    }
    auto it = sym->structure->fields.find(expr.field);
    if (it == sym->structure->fields.end()) {
        addDiagnostic(expr, "поле '" + expr.field + "' не существует в " + objType.toString());
        return Type::error();
    }
    return it->second;
}

Type Analyzer::analyzeIndex(AST::IndexExpr& expr) {
    Type objType = analyzeExpr(*expr.object);
    Type idxType = analyzeExpr(*expr.index);
    if (objType.isError()) return Type::error();
    if (objType.kind != Type::Kind::Array) {
        addDiagnostic(expr, "индексирование применимо только к массивам, получен " + objType.toString());
        return Type::error();
    }
    if (!idxType.isError() && idxType.kind != Type::Kind::Int && idxType.kind != Type::Kind::UInt)
        addDiagnostic(*expr.index, "индекс массива должен быть целым числом, получен " + idxType.toString());
    return objType.elementType ? *objType.elementType : Type::error();
}

Analyzer::LValueInfo Analyzer::analyzeLValue(AST::Expr& expr) {
    LValueInfo info;
    if (auto* name = dynamic_cast<AST::NameExpr*>(&expr)) {
        Symbol* sym = resolvePath(name->path, expr);
        if (!sym || sym->kind != SymbolKind::Variable) return info;
        info.isLValue = true;
        info.isMutable = sym->isMutable;
        info.type = sym->type;
        setType(expr, sym->type);
        return info;
    }
    if (auto* field = dynamic_cast<AST::FieldExpr*>(&expr)) {
        LValueInfo base = analyzeLValue(*field->object);
        if (!base.isLValue) return info;
        Type ft = analyzeField(*field);
        info.isLValue = true;
        info.isMutable = base.isMutable;
        info.type = ft;
        return info;
    }
    if (auto* idx = dynamic_cast<AST::IndexExpr*>(&expr)) {
        LValueInfo base = analyzeLValue(*idx->object);
        if (!base.isLValue) return info;
        Type et = analyzeIndex(*idx);
        info.isLValue = true;
        info.isMutable = base.isMutable;
        info.type = et;
        return info;
    }
    analyzeExpr(expr);
    return info;
}


int Analyzer::implicitConversionScore(const Type& lhs, const Type& rhs) const {
    if (lhs == rhs) return 0;
    if ((lhs.kind == Type::Kind::Int || lhs.kind == Type::Kind::UInt) &&
        (rhs.kind == Type::Kind::Int || rhs.kind == Type::Kind::UInt) &&
        rhs.name == "int32") return 1;
    if (lhs.kind == Type::Kind::Float && rhs.kind == Type::Kind::Float && rhs.name == "float64") return 1;
    return -1;
}

bool Analyzer::canAssignSilently(const Type& lhs, const Type& rhs) const {
    if (lhs == rhs) return true;
    if ((lhs.kind == Type::Kind::Int || lhs.kind == Type::Kind::UInt) &&
        (rhs.kind == Type::Kind::Int || rhs.kind == Type::Kind::UInt) &&
        rhs.name == "int32") return true;
    if (lhs.kind == Type::Kind::Float && rhs.kind == Type::Kind::Float && rhs.name == "float64") return true;
    return false;
}

bool Analyzer::checkAssignable(const Type& lhs, const Type& rhs, const AST::Node& node) {
    if (lhs == rhs) return true;
    if ((lhs.kind == Type::Kind::Int || lhs.kind == Type::Kind::UInt) &&
        (rhs.kind == Type::Kind::Int || rhs.kind == Type::Kind::UInt) &&
        rhs.name == "int32") {
        return true;
    }
    if (lhs.kind == Type::Kind::Float && rhs.kind == Type::Kind::Float && rhs.name == "float64") {
        return true;
    }
    addDiagnostic(node, "несовместимые типы: ожидается " + lhs.toString() + ", получен " + rhs.toString());
    return false;
}

bool Analyzer::canCast(const Type& from, const Type& to) const {
    if (from == to) return true;
    if (from.isNumeric() && to.isNumeric()) return true;
    if (from.kind == Type::Kind::Bool && to.isNumeric()) return true;
    if (from.isNumeric() && to.kind == Type::Kind::Bool) return true;
    if (from.kind == Type::Kind::Char && to.isNumeric()) return true;
    if (from.isNumeric() && to.kind == Type::Kind::Char) return true;
    return false;
}

bool Analyzer::isPrintable(const Type& type) const {
    return type.kind == Type::Kind::Int   ||
           type.kind == Type::Kind::UInt  ||
           type.kind == Type::Kind::Float ||
           type.kind == Type::Kind::Bool  ||
           type.kind == Type::Kind::Char  ||
           type.kind == Type::Kind::String;
}

bool Analyzer::isTerminatingCall(const AST::Expr& expr) const {
    const auto* call = dynamic_cast<const AST::CallExpr*>(&expr);
    if (!call) return false;
    const auto* name = dynamic_cast<const AST::NameExpr*>(call->callee.get());
    if (!name || name->path.size() != 1) return false;
    return name->path[0] == "exit" || name->path[0] == "panic";
}

std::string formatDiagnostic(const Diagnostic& d) {
    return d.file + ":" + std::to_string(d.pos.line) + ":" +
           std::to_string(d.pos.column) + ": error: " + d.message;
}
}
