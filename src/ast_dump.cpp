#include "ast_dump.h"

#include <string>

namespace ASTDump {

namespace {

std::string ind(int n) { return std::string(static_cast<std::size_t>(n), ' '); }

std::string join(const std::vector<std::string>& path) {
    std::string r;
    for (const auto& p : path) {
        if (!r.empty()) r += "::";
        r += p;
    }
    return r;
}

void dumpType(const AST::TypeExpr* t, std::ostream& out) {
    if (!t) { out << "<inferred>"; return; }
    if (auto* n = dynamic_cast<const AST::NamedType*>(t)) { out << join(n->path); return; }
    if (auto* a = dynamic_cast<const AST::ArrayType*>(t)) {
        out << "[";
        dumpType(a->elementType.get(), out);
        out << "; " << a->size << "]";
        return;
    }
    out << "<type>";
}

void dumpExpr(const AST::Expr* e, std::ostream& out, int pad);
void dumpStmt(const AST::Stmt* s, std::ostream& out, int pad);
void dumpDecl(const AST::Decl* d, std::ostream& out, int pad);

void dumpExprHeader(const AST::Expr* e, std::ostream& out, int pad, const std::string& name) {
    out << ind(pad) << name;
    if (e && e->semanticType) out << " : " << *e->semanticType;
    out << "\n";
}

void dumpExpr(const AST::Expr* e, std::ostream& out, int pad) {
    if (!e) { out << ind(pad) << "<null expr>\n"; return; }
    if (auto* x = dynamic_cast<const AST::IntLiteralExpr*>(e)) { dumpExprHeader(e,out,pad,"IntLiteral " + x->lexeme); return; }
    if (auto* x = dynamic_cast<const AST::FloatLiteralExpr*>(e)) { dumpExprHeader(e,out,pad,"FloatLiteral " + x->lexeme); return; }
    if (auto* x = dynamic_cast<const AST::BoolLiteralExpr*>(e)) { dumpExprHeader(e,out,pad,std::string("BoolLiteral ") + (x->value ? "true" : "false")); return; }
    if (auto* x = dynamic_cast<const AST::StringLiteralExpr*>(e)) { dumpExprHeader(e,out,pad,"StringLiteral \"" + x->value + "\""); return; }
    if (auto* x = dynamic_cast<const AST::NameExpr*>(e)) { dumpExprHeader(e,out,pad,"Name " + join(x->path)); return; }
    if (auto* x = dynamic_cast<const AST::UnaryExpr*>(e)) { dumpExprHeader(e,out,pad,"Unary " + x->op); dumpExpr(x->operand.get(),out,pad+2); return; }
    if (auto* x = dynamic_cast<const AST::BinaryExpr*>(e)) { dumpExprHeader(e,out,pad,"Binary " + x->op); dumpExpr(x->left.get(),out,pad+2); dumpExpr(x->right.get(),out,pad+2); return; }
    if (auto* x = dynamic_cast<const AST::CastExpr*>(e)) { dumpExprHeader(e,out,pad,"Cast"); out << ind(pad+2) << "to "; dumpType(x->targetType.get(), out); out << "\n"; dumpExpr(x->value.get(),out,pad+2); return; }
    if (auto* x = dynamic_cast<const AST::CallExpr*>(e)) { dumpExprHeader(e,out,pad,"Call"); dumpExpr(x->callee.get(),out,pad+2); for (auto& a: x->args) dumpExpr(a.get(),out,pad+2); return; }
    if (auto* x = dynamic_cast<const AST::FieldExpr*>(e)) { dumpExprHeader(e,out,pad,"Field ." + x->field); dumpExpr(x->object.get(),out,pad+2); return; }
    if (auto* x = dynamic_cast<const AST::IndexExpr*>(e)) { dumpExprHeader(e,out,pad,"Index"); dumpExpr(x->object.get(),out,pad+2); dumpExpr(x->index.get(),out,pad+2); return; }
    if (auto* x = dynamic_cast<const AST::ArrayLiteralExpr*>(e)) { dumpExprHeader(e,out,pad,"ArrayLiteral"); for (auto& a: x->elements) dumpExpr(a.get(),out,pad+2); return; }
    if (auto* x = dynamic_cast<const AST::StructLiteralExpr*>(e)) { dumpExprHeader(e,out,pad,"StructLiteral " + join(x->typePath)); for (auto& f: x->fields) { out << ind(pad+2) << f.name << "\n"; dumpExpr(f.value.get(),out,pad+4);} return; }
    dumpExprHeader(e,out,pad,"<expr>");
}

void dumpBlock(const AST::BlockStmt* b, std::ostream& out, int pad) {
    out << ind(pad) << "Block\n";
    for (auto& s : b->statements) dumpStmt(s.get(), out, pad + 2);
}

void dumpStmt(const AST::Stmt* s, std::ostream& out, int pad) {
    if (dynamic_cast<const AST::EmptyStmt*>(s)) { out << ind(pad) << "EmptyStmt\n"; return; }
    if (auto* b = dynamic_cast<const AST::BlockStmt*>(s)) return dumpBlock(b,out,pad);
    if (auto* x = dynamic_cast<const AST::LetStmt*>(s)) { out << ind(pad) << "Let " << x->name << ": "; dumpType(x->explicitType.get(),out); out << "\n"; dumpExpr(x->initializer.get(),out,pad+2); return; }
    if (auto* x = dynamic_cast<const AST::VarStmt*>(s)) { out << ind(pad) << "Var " << x->name << ": "; dumpType(x->explicitType.get(),out); out << "\n"; dumpExpr(x->initializer.get(),out,pad+2); return; }
    if (auto* x = dynamic_cast<const AST::AssignStmt*>(s)) { out << ind(pad) << "Assign\n"; dumpExpr(x->target.get(),out,pad+2); dumpExpr(x->value.get(),out,pad+2); return; }
    if (auto* x = dynamic_cast<const AST::ExprStmt*>(s)) { out << ind(pad) << "ExprStmt\n"; dumpExpr(x->expr.get(),out,pad+2); return; }
    if (auto* x = dynamic_cast<const AST::IfStmt*>(s)) { out << ind(pad) << "If\n"; dumpExpr(x->condition.get(),out,pad+2); dumpBlock(x->thenBlock.get(),out,pad+2); if (x->elseBranch) dumpStmt(x->elseBranch.get(),out,pad+2); return; }
    if (auto* x = dynamic_cast<const AST::WhileStmt*>(s)) { out << ind(pad) << "While\n"; dumpExpr(x->condition.get(),out,pad+2); dumpBlock(x->body.get(),out,pad+2); return; }
    if (dynamic_cast<const AST::BreakStmt*>(s)) { out << ind(pad) << "Break\n"; return; }
    if (dynamic_cast<const AST::ContinueStmt*>(s)) { out << ind(pad) << "Continue\n"; return; }
    if (auto* x = dynamic_cast<const AST::ReturnStmt*>(s)) { out << ind(pad) << "Return\n"; dumpExpr(x->value.get(),out,pad+2); return; }
    out << ind(pad) << "<stmt>\n";
}

void dumpDecl(const AST::Decl* d, std::ostream& out, int pad) {
    if (auto* ns = dynamic_cast<const AST::NamespaceDecl*>(d)) { out << ind(pad) << "Namespace " << ns->name << "\n"; for (auto& c: ns->decls) dumpDecl(c.get(),out,pad+2); return; }
    if (auto* t = dynamic_cast<const AST::TypeAliasDecl*>(d)) { out << ind(pad) << "TypeAlias " << t->name << " = "; dumpType(t->aliasedType.get(),out); out << "\n"; return; }
    if (auto* s = dynamic_cast<const AST::StructDecl*>(d)) { out << ind(pad) << "Struct " << s->name << "\n"; for (auto& f: s->fields) { out << ind(pad+2) << f.name << ": "; dumpType(f.type.get(),out); out << "\n"; } return; }
    if (auto* f = dynamic_cast<const AST::FunctionDecl*>(d)) { out << ind(pad) << "Function " << f->name << " -> "; dumpType(f->returnType.get(),out); out << "\n"; for (auto& p: f->params) { out << ind(pad+2) << "Param " << p.name << ": "; dumpType(p.type.get(),out); out << "\n"; } dumpBlock(f->body.get(),out,pad+2); return; }
    out << ind(pad) << "<decl>\n";
}

} // namespace

void dumpModule(const AST::Module& module, std::ostream& out) {
    out << "Module";
    if (!module.namePath.empty()) out << " " << join(module.namePath);
    out << "\n";
    for (auto& d : module.decls) dumpDecl(d.get(), out, 2);
}

} // namespace ASTDump
