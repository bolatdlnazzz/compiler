#include "parser.h"

#include <charconv>
#include <sstream>
#include <utility>

namespace Parser {
Parser::Parser(std::vector<Lexer::Token> tokens, std::string fileName, Options options)
    : tokens_(std::move(tokens)), fileName_(std::move(fileName)), options_(options) {
    if (tokens_.empty() || tokens_.back().type != Lexer::TokenType::EndOfFile) {
        tokens_.push_back(Lexer::Token{Lexer::TokenType::EndOfFile, "", Lexer::Position{1, 1}});
    }
}

const Lexer::Token& Parser::peek(std::size_t lookahead) const {
    const std::size_t index = current_ + lookahead;
    if (index >= tokens_.size()) return tokens_.back();
    return tokens_[index];
}

const Lexer::Token& Parser::previous() const {
    if (current_ == 0) return tokens_.front();
    return tokens_[current_ - 1];
}

bool Parser::isAtEnd() const {
    return peek().type == Lexer::TokenType::EndOfFile;
}

const Lexer::Token& Parser::advance() {
    if (!isAtEnd()) ++current_;
    return previous();
}

bool Parser::check(Lexer::TokenType type, std::string_view lexeme) const {
    if (peek().type != type) return false;
    return lexeme.empty() || peek().lexeme == lexeme;
}

bool Parser::match(Lexer::TokenType type, std::string_view lexeme) {
    if (!check(type, lexeme)) return false;
    advance();
    return true;
}

const Lexer::Token& Parser::expect(Lexer::TokenType type,
                                   std::string_view lexeme,
                                   std::string_view message) {
    if (check(type, lexeme)) return advance();
    errorAt(peek(), message);
    panicMode_ = true;
    return peek();
}

void Parser::errorAt(const Lexer::Token& tok, std::string_view message) {
    if (panicMode_) return;
    diagnostics_.push_back(Diagnostic{fileName_, tok.pos, std::string(message)});
}

void Parser::synchronizeTopLevel() {
    panicMode_ = false;
    while (!isAtEnd()) {
        if (check(Lexer::TokenType::Keyword, "module") ||
            check(Lexer::TokenType::Keyword, "namespace") ||
            check(Lexer::TokenType::Keyword, "type") ||
            check(Lexer::TokenType::Keyword, "struct") ||
            check(Lexer::TokenType::Keyword, "fn")) {
            return;
        }
        advance();
    }
}

void Parser::synchronizeStatement() {
    panicMode_ = false;
    while (!isAtEnd()) {
        if (previous().lexeme == ";") return;
        if (check(Lexer::TokenType::Keyword, "let") ||
            check(Lexer::TokenType::Keyword, "var") ||
            check(Lexer::TokenType::Keyword, "if") ||
            check(Lexer::TokenType::Keyword, "while") ||
            check(Lexer::TokenType::Keyword, "break") ||
            check(Lexer::TokenType::Keyword, "continue") ||
            check(Lexer::TokenType::Keyword, "return") ||
            check(Lexer::TokenType::Separator, "}")) {
            return;
        }
        advance();
    }
}

bool Parser::isNameLike(const Lexer::Token& tok) const {
    if (tok.type == Lexer::TokenType::Identifier) return true;
    if (tok.type != Lexer::TokenType::Keyword) return false;
    return tok.lexeme == "print" || tok.lexeme == "input" || tok.lexeme == "exit" ||
           tok.lexeme == "panic" || tok.lexeme == "assert" || tok.lexeme == "unit" || tok.lexeme == "len";
}

bool Parser::isTypeNameLike(const Lexer::Token& tok) const {
    return tok.type == Lexer::TokenType::Identifier ||
           (tok.type == Lexer::TokenType::Keyword && tok.lexeme == "unit");
}

std::string Parser::expectIdentifierLike(std::string_view message) {
    if (peek().type == Lexer::TokenType::Identifier) return advance().lexeme;
    if (peek().type == Lexer::TokenType::Keyword &&
        (peek().lexeme == "print" || peek().lexeme == "input" ||
         peek().lexeme == "exit" || peek().lexeme == "panic" || peek().lexeme == "assert" || peek().lexeme == "len")) {
        return advance().lexeme;
    }
    errorAt(peek(), message);
    panicMode_ = true;
    return "<error>";
}

std::vector<std::string> Parser::parseNamePath() {
    std::vector<std::string> path;
    if (!isNameLike(peek()) && !isTypeNameLike(peek())) {
        errorAt(peek(), "ожидалось имя");
        panicMode_ = true;
        return {"<error>"};
    }
    path.push_back(advance().lexeme);
    while (match(Lexer::TokenType::Operator, "::")) {
        if (!isNameLike(peek()) && !isTypeNameLike(peek())) {
            errorAt(peek(), "ожидалось имя после '::'");
            panicMode_ = true;
            break;
        }
        path.push_back(advance().lexeme);
    }
    return path;
}

std::vector<std::string> Parser::parseModuleNamePath() {
    std::vector<std::string> path;
    path.push_back(expectIdentifierLike("ожидалось имя модуля"));
    while (match(Lexer::TokenType::Operator, "::")) {
        path.push_back(expectIdentifierLike("ожидалось имя модуля после '::'"));
    }
    return path;
}

AST::SourceSpan Parser::spanFrom(const Lexer::Token& first, const Lexer::Token& last) const {
    return AST::SourceSpan{fileName_, first.pos, last.pos};
}

std::string Parser::decodeStringLiteral(std::string_view lexeme) {
    std::string result;
    if (lexeme.size() < 2) return result;
    for (std::size_t i = 1; i + 1 < lexeme.size(); ++i) {
        char c = lexeme[i];
        if (c == '\\' && i + 1 < lexeme.size() - 1) {
            char escaped = lexeme[++i];
            switch (escaped) {
                case 'n': result.push_back('\n'); break;
                case 't': result.push_back('\t'); break;
                case '"': result.push_back('"'); break;
                case '\\': result.push_back('\\'); break;
                default: result.push_back(escaped); break;
            }
        } else {
            result.push_back(c);
        }
    }
    return result;
}

char Parser::decodeCharLiteral(std::string_view lexeme) {
    if (lexeme.size() < 3) return '\0';
    if (lexeme[1] != '\\') return lexeme[1];
    if (lexeme.size() < 4) return '\0';
    switch (lexeme[2]) {
        case 'n': return '\n';
        case 't': return '\t';
        case '\'': return '\'';
        case '\\': return '\\';
        case '0': return '\0';
        default: return lexeme[2];
    }
}

std::unique_ptr<AST::Module> Parser::parseModule() {
    auto module = std::make_unique<AST::Module>();
    const auto first = peek();
    if (match(Lexer::TokenType::Keyword, "module")) {
        module->namePath = parseModuleNamePath();
        expect(Lexer::TokenType::Separator, ";", "ожидался ';' после объявления module");
        panicMode_ = false;
    }
    while (!isAtEnd()) {
        if (check(Lexer::TokenType::Keyword, "module")) {
            errorAt(peek(), "объявление module допустимо только в начале файла");
            break;
        }
        if (panicMode_) {
            synchronizeTopLevel();
            continue;
        }
        AST::DeclPtr decl = parseDeclaration();
        if (decl) module->decls.push_back(std::move(decl));
        if (panicMode_) {
            if (!options_.recoverErrors) break;
            synchronizeTopLevel();
        }
    }
    module->span = module->decls.empty()
        ? spanFrom(first, peek())
        : spanFrom(first, previous());
    return module;
}

AST::DeclPtr Parser::parseDeclaration() {
    if (match(Lexer::TokenType::Keyword, "namespace")) return parseNamespaceDecl();
    if (match(Lexer::TokenType::Keyword, "type"))      return parseTypeAliasDecl();
    if (match(Lexer::TokenType::Keyword, "struct"))    return parseStructDecl();
    if (match(Lexer::TokenType::Keyword, "fn"))        return parseFunctionDecl();
    errorAt(peek(), "ожидалось верхнеуровневое объявление: namespace, type, struct или fn");
    panicMode_ = true;
    return nullptr;
}

std::unique_ptr<AST::NamespaceDecl> Parser::parseNamespaceDecl() {
    const auto first = previous();
    auto node = std::make_unique<AST::NamespaceDecl>();
    node->name = expectIdentifierLike("ожидалось имя пространства имён");
    if (panicMode_) return node;
    expect(Lexer::TokenType::Separator, "{", "ожидался '{' после имени namespace");
    if (panicMode_) return node;
    while (!isAtEnd() && !check(Lexer::TokenType::Separator, "}")) {
        if (panicMode_) { synchronizeTopLevel(); continue; }
        AST::DeclPtr decl = parseDeclaration();
        if (decl) node->decls.push_back(std::move(decl));
        if (panicMode_ && !options_.recoverErrors) return node;
        if (panicMode_) synchronizeTopLevel();
    }
    expect(Lexer::TokenType::Separator, "}", "ожидался '}' после namespace");
    panicMode_ = false;
    node->span = spanFrom(first, previous());
    return node;
}

std::unique_ptr<AST::TypeAliasDecl> Parser::parseTypeAliasDecl() {
    const auto first = previous();
    auto node = std::make_unique<AST::TypeAliasDecl>();
    node->name = expectIdentifierLike("ожидалось имя type alias");
    if (panicMode_) return node;
    expect(Lexer::TokenType::Operator, "=", "ожидался '=' в объявлении type alias");
    if (panicMode_) return node;
    node->aliasedType = parseTypeExpr();
    if (panicMode_) return node;
    expect(Lexer::TokenType::Separator, ";", "ожидался ';' после type alias");
    panicMode_ = false;
    node->span = spanFrom(first, previous());
    return node;
}

std::unique_ptr<AST::StructDecl> Parser::parseStructDecl() {
    const auto first = previous();
    auto node = std::make_unique<AST::StructDecl>();
    node->name = expectIdentifierLike("ожидалось имя структуры");
    if (panicMode_) return node;
    expect(Lexer::TokenType::Separator, "{", "ожидался '{' после имени структуры");
    if (panicMode_) return node;
    while (!isAtEnd() && !check(Lexer::TokenType::Separator, "}")) {
        if (panicMode_) { synchronizeStatement(); continue; }
        AST::FieldDecl field;
        const auto fieldFirst = peek();
        field.name = expectIdentifierLike("ожидалось имя поля структуры");
        if (panicMode_) { synchronizeStatement(); continue; }
        expect(Lexer::TokenType::Separator, ":", "ожидался ':' после имени поля");
        if (panicMode_) { synchronizeStatement(); continue; }
        field.type = parseTypeExpr();
        if (panicMode_) { synchronizeStatement(); continue; }
        expect(Lexer::TokenType::Separator, ";", "ожидался ';' после поля структуры");
        panicMode_ = false;
        field.span = spanFrom(fieldFirst, previous());
        node->fields.push_back(std::move(field));
    }
    expect(Lexer::TokenType::Separator, "}", "ожидался '}' после структуры");
    panicMode_ = false;
    node->span = spanFrom(first, previous());
    return node;
}

std::unique_ptr<AST::FunctionDecl> Parser::parseFunctionDecl() {
    const auto first = previous();
    auto node = std::make_unique<AST::FunctionDecl>();
    node->name = expectIdentifierLike("ожидалось имя функции");
    if (panicMode_) return node;
    expect(Lexer::TokenType::Separator, "(", "ожидался '(' после имени функции");
    if (panicMode_) return node;
    if (!check(Lexer::TokenType::Separator, ")")) {
        do {
            if (panicMode_) break;
            AST::Param param;
            const auto paramFirst = peek();
            param.name = expectIdentifierLike("ожидалось имя параметра");
            if (panicMode_) break;
            expect(Lexer::TokenType::Separator, ":", "ожидался ':' перед типом параметра");
            if (panicMode_) break;
            param.type = parseTypeExpr();
            if (panicMode_) break;
            param.span = spanFrom(paramFirst, previous());
            node->params.push_back(std::move(param));
        } while (!panicMode_ && match(Lexer::TokenType::Separator, ","));
    }
    if (panicMode_) return node;
    expect(Lexer::TokenType::Separator, ")", "ожидался ')' после списка параметров");
    if (panicMode_) return node;
    if (match(Lexer::TokenType::Separator, "->")) {
        node->returnType = parseTypeExpr();
        if (panicMode_) return node;
    }
    node->body = parseBlock();
    panicMode_ = false;
    node->span = spanFrom(first, previous());
    return node;
}

AST::TypePtr Parser::parseTypeExpr() {
    const auto first = peek();
    if (match(Lexer::TokenType::Separator, "[")) {
        auto node = std::make_unique<AST::ArrayType>();
        node->elementType = parseTypeExpr();
        if (panicMode_) return node;
        expect(Lexer::TokenType::Separator, ";", "ожидался ';' между типом элемента и размером массива");
        if (panicMode_) return node;
        const auto& sizeTok = expect(Lexer::TokenType::IntLiteral, {}, "ожидался целочисленный размер массива");
        if (panicMode_) return node;
        std::uint64_t size = 0;
        int base = 10;
        std::string_view digits = sizeTok.lexeme;
        if (digits.size() > 2 && digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) {
            base = 16;
            digits.remove_prefix(2);
        } else if (digits.size() > 2 && digits[0] == '0' && (digits[1] == 'b' || digits[1] == 'B')) {
            base = 2;
            digits.remove_prefix(2);
        }
        auto begin = digits.data();
        auto end = digits.data() + digits.size();
        auto [ptr, ec] = std::from_chars(begin, end, size, base);
        if (ec != std::errc{} || ptr != end) {
            errorAt(sizeTok, "некорректный размер массива");
        }
        node->size = size;
        expect(Lexer::TokenType::Separator, "]", "ожидался ']' после типа массива");
        panicMode_ = false;
        node->span = spanFrom(first, previous());
        return node;
    }
    if (!isTypeNameLike(peek())) {
        errorAt(peek(), "ожидался тип");
        panicMode_ = true;
        auto err = std::make_unique<AST::NamedType>();
        err->path = {"<error>"};
        return err;
    }
    auto node = std::make_unique<AST::NamedType>();
    node->path.push_back(advance().lexeme);
    while (match(Lexer::TokenType::Operator, "::")) {
        if (!isTypeNameLike(peek())) {
            errorAt(peek(), "ожидалось имя типа после '::'");
            panicMode_ = true;
            break;
        }
        node->path.push_back(advance().lexeme);
    }
    node->span = spanFrom(first, previous());
    return node;
}

AST::StmtPtr Parser::parseStatement() {
    if (match(Lexer::TokenType::Separator, ";")) {
        auto s = std::make_unique<AST::EmptyStmt>();
        s->span = spanFrom(previous(), previous());
        return s;
    }
    if (match(Lexer::TokenType::Keyword, "let"))      return parseLetStmt();
    if (match(Lexer::TokenType::Keyword, "var"))      return parseVarStmt();
    if (match(Lexer::TokenType::Keyword, "if"))       return parseIfStmt();
    if (match(Lexer::TokenType::Keyword, "while"))    return parseWhileStmt();
    if (match(Lexer::TokenType::Keyword, "break")) {
        const auto first = previous();
        auto s = std::make_unique<AST::BreakStmt>();
        expect(Lexer::TokenType::Separator, ";", "ожидался ';' после break");
        panicMode_ = false;
        s->span = spanFrom(first, previous());
        return s;
    }
    if (match(Lexer::TokenType::Keyword, "continue")) {
        const auto first = previous();
        auto s = std::make_unique<AST::ContinueStmt>();
        expect(Lexer::TokenType::Separator, ";", "ожидался ';' после continue");
        panicMode_ = false;
        s->span = spanFrom(first, previous());
        return s;
    }
    if (match(Lexer::TokenType::Keyword, "return")) return parseReturnStmt();
    if (check(Lexer::TokenType::Separator, "{"))    return parseBlock();
    const auto first = peek();
    auto lhsOrExpr = parseExpression();
    if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    if (match(Lexer::TokenType::Operator, "=")) {
        if (!isAssignable(*lhsOrExpr)) {
            errorAt(previous(), "левая часть присваивания должна быть lvalue (имя переменной, поле, элемент массива)");
        }
        auto value = parseExpression();
        if (panicMode_) return std::make_unique<AST::EmptyStmt>();
        expect(Lexer::TokenType::Separator, ";", "ожидался ';' после присваивания");
        panicMode_ = false;
        auto s = std::make_unique<AST::AssignStmt>();
        s->target = std::move(lhsOrExpr);
        s->value = std::move(value);
        s->span = spanFrom(first, previous());
        return s;
    }
    expect(Lexer::TokenType::Separator, ";", "ожидался ';' после выражения");
    panicMode_ = false;
    auto s = std::make_unique<AST::ExprStmt>();
    s->expr = std::move(lhsOrExpr);
    s->span = spanFrom(first, previous());
    return s;
}

std::unique_ptr<AST::BlockStmt> Parser::parseBlock() {
    const auto first = expect(Lexer::TokenType::Separator, "{", "ожидался '{' в начале блока");
    if (panicMode_) {
        auto block = std::make_unique<AST::BlockStmt>();
        block->span = spanFrom(first, first);
        return block;
    }
    auto block = std::make_unique<AST::BlockStmt>();
    while (!isAtEnd() && !check(Lexer::TokenType::Separator, "}")) {
        if (panicMode_) {
            synchronizeStatement();
            panicMode_ = false;
            continue;
        }
        AST::StmtPtr stmt = parseStatement();
        if (stmt) block->statements.push_back(std::move(stmt));
        if (panicMode_) {
            if (!options_.recoverErrors) break;
            synchronizeStatement();
            panicMode_ = false;
        }
    }
    expect(Lexer::TokenType::Separator, "}", "ожидался '}' в конце блока");
    panicMode_ = false;
    block->span = spanFrom(first, previous());
    return block;
}

AST::StmtPtr Parser::parseLetStmt() {
    const auto first = previous();
    auto s = std::make_unique<AST::LetStmt>();
    s->name = expectIdentifierLike("ожидалось имя переменной после let");
    if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    if (match(Lexer::TokenType::Separator, ":")) {
        s->explicitType = parseTypeExpr();
        if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    }
    expect(Lexer::TokenType::Operator, "=", "ожидался '=' в объявлении let");
    if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    s->initializer = parseExpression();
    if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    expect(Lexer::TokenType::Separator, ";", "ожидался ';' после объявления let");
    panicMode_ = false;
    s->span = spanFrom(first, previous());
    return s;
}

AST::StmtPtr Parser::parseVarStmt() {
    const auto first = previous();
    auto s = std::make_unique<AST::VarStmt>();
    s->name = expectIdentifierLike("ожидалось имя переменной после var");
    if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    expect(Lexer::TokenType::Separator, ":", "var требует явного типа и ':' после имени");
    if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    s->explicitType = parseTypeExpr();
    if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    expect(Lexer::TokenType::Operator, "=", "ожидался '=' в объявлении var");
    if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    s->initializer = parseExpression();
    if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    expect(Lexer::TokenType::Separator, ";", "ожидался ';' после объявления var");
    panicMode_ = false;
    s->span = spanFrom(first, previous());
    return s;
}

AST::StmtPtr Parser::parseIfStmt() {
    const auto first = previous();
    auto s = std::make_unique<AST::IfStmt>();
    expect(Lexer::TokenType::Separator, "(", "ожидался '(' после if");
    if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    s->condition = parseExpression();
    if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    expect(Lexer::TokenType::Separator, ")", "ожидался ')' после условия if");
    if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    s->thenBlock = parseBlock();
    if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    if (match(Lexer::TokenType::Keyword, "else")) {
        if (check(Lexer::TokenType::Separator, "{")) {
            s->elseBranch = parseBlock();
        } else if (match(Lexer::TokenType::Keyword, "if")) {
            s->elseBranch = parseIfStmt();
        } else {
            errorAt(peek(), "ожидался блок или if после else");
            panicMode_ = true;
            return std::make_unique<AST::EmptyStmt>();
        }
    }
    panicMode_ = false;
    s->span = spanFrom(first, previous());
    return s;
}

AST::StmtPtr Parser::parseWhileStmt() {
    const auto first = previous();
    auto s = std::make_unique<AST::WhileStmt>();
    expect(Lexer::TokenType::Separator, "(", "ожидался '(' после while");
    if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    s->condition = parseExpression();
    if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    expect(Lexer::TokenType::Separator, ")", "ожидался ')' после условия while");
    if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    s->body = parseBlock();
    panicMode_ = false;
    s->span = spanFrom(first, previous());
    return s;
}

AST::StmtPtr Parser::parseReturnStmt() {
    const auto first = previous();
    auto s = std::make_unique<AST::ReturnStmt>();
    if (!check(Lexer::TokenType::Separator, ";")) {
        s->value = parseExpression();
        if (panicMode_) return std::make_unique<AST::EmptyStmt>();
    }
    expect(Lexer::TokenType::Separator, ";", "ожидался ';' после return");
    panicMode_ = false;
    s->span = spanFrom(first, previous());
    return s;
}

AST::ExprPtr Parser::parseExpression(int minPrec) {
    auto left = parseUnary();
    if (panicMode_) return left;
    while (true) {
        const Lexer::Token& opTok = peek();
        const int prec = precedenceOf(opTok);
        if (prec < minPrec) break;
        advance();
        if (opTok.type == Lexer::TokenType::Keyword && opTok.lexeme == "as") {
            auto target = parseTypeExpr();
            if (panicMode_) return left;
            auto cast = std::make_unique<AST::CastExpr>();
            cast->value = std::move(left);
            cast->targetType = std::move(target);
            cast->span = spanFrom(opTok, previous());
            left = std::move(cast);
            continue;
        }
        const int nextMinPrec = isLeftAssociative(opTok) ? prec + 1 : prec;
        auto right = parseExpression(nextMinPrec);
        if (panicMode_) return left;
        auto binary = std::make_unique<AST::BinaryExpr>();
        binary->op = opTok.lexeme;
        binary->left = std::move(left);
        binary->right = std::move(right);
        binary->span = spanFrom(opTok, previous());
        left = std::move(binary);
    }
    return left;
}

AST::ExprPtr Parser::parseUnary() {
    if (check(Lexer::TokenType::Operator, "-") || check(Lexer::TokenType::Operator, "!")) {
        const auto opTok = advance();
        auto node = std::make_unique<AST::UnaryExpr>();
        node->op = opTok.lexeme;
        node->operand = parseUnary();
        node->span = spanFrom(opTok, previous());
        return node;
    }
    return parsePostfix();
}

AST::ExprPtr Parser::parsePostfix() {
    auto expr = parsePrimary();
    if (panicMode_) return expr;
    while (true) {
        if (panicMode_) break;
        if (match(Lexer::TokenType::Separator, "(")) {
            const auto first = previous();
            auto call = std::make_unique<AST::CallExpr>();
            call->callee = std::move(expr);
            if (!check(Lexer::TokenType::Separator, ")")) {
                do {
                    if (panicMode_) break;
                    call->args.push_back(parseExpression());
                } while (!panicMode_ && match(Lexer::TokenType::Separator, ","));
            }
            if (panicMode_) return call;
            expect(Lexer::TokenType::Separator, ")", "ожидался ')' после аргументов вызова");
            panicMode_ = false;
            call->span = spanFrom(first, previous());
            expr = std::move(call);
            continue;
        }
        if (match(Lexer::TokenType::Operator, ".")) {
            const auto first = previous();
            auto field = std::make_unique<AST::FieldExpr>();
            field->object = std::move(expr);
            field->field = expectIdentifierLike("ожидалось имя поля после '.'");
            if (panicMode_) return field;
            field->span = spanFrom(first, previous());
            expr = std::move(field);
            continue;
        }
        if (match(Lexer::TokenType::Separator, "[")) {
            const auto first = previous();
            auto index = std::make_unique<AST::IndexExpr>();
            index->object = std::move(expr);
            index->index = parseExpression();
            if (panicMode_) return index;
            expect(Lexer::TokenType::Separator, "]", "ожидался ']' после индекса");
            panicMode_ = false;
            index->span = spanFrom(first, previous());
            expr = std::move(index);
            continue;
        }
        if (match(Lexer::TokenType::Operator, "::")) {
            if (auto* name = dynamic_cast<AST::NameExpr*>(expr.get())) {
                std::string next = expectIdentifierLike("ожидалось имя после '::'");
                if (panicMode_) return expr;
                name->path.push_back(std::move(next));
                continue;
            }
            errorAt(previous(), "оператор '::' допустим только для имени");
            panicMode_ = true;
            break;
        }
        break;
    }
    return expr;
}

AST::ExprPtr Parser::parsePrimary() {
    const auto first = peek();
    if (match(Lexer::TokenType::IntLiteral)) {
        auto e = std::make_unique<AST::IntLiteralExpr>();
        e->lexeme = previous().lexeme;
        e->span = spanFrom(first, previous());
        return e;
    }
    if (match(Lexer::TokenType::FloatLiteral)) {
        auto e = std::make_unique<AST::FloatLiteralExpr>();
        e->lexeme = previous().lexeme;
        e->span = spanFrom(first, previous());
        return e;
    }
    if (match(Lexer::TokenType::BoolLiteral)) {
        auto e = std::make_unique<AST::BoolLiteralExpr>();
        e->value = previous().lexeme == "true";
        e->span = spanFrom(first, previous());
        return e;
    }
    if (match(Lexer::TokenType::CharLiteral)) {
        auto e = std::make_unique<AST::CharLiteralExpr>();
        e->value = decodeCharLiteral(previous().lexeme);
        e->span = spanFrom(first, previous());
        return e;
    }
    if (match(Lexer::TokenType::StringLiteral)) {
        auto e = std::make_unique<AST::StringLiteralExpr>();
        e->value = decodeStringLiteral(previous().lexeme);
        e->span = spanFrom(first, previous());
        return e;
    }
    if (match(Lexer::TokenType::Separator, "(")) {
        auto expr = parseExpression();
        if (panicMode_) return expr;
        expect(Lexer::TokenType::Separator, ")", "ожидался ')' после выражения");
        panicMode_ = false;
        return expr;
    }
    if (match(Lexer::TokenType::Separator, "[")) {
        auto e = std::make_unique<AST::ArrayLiteralExpr>();
        if (!check(Lexer::TokenType::Separator, "]")) {
            do {
                if (panicMode_) break;
                e->elements.push_back(parseExpression());
            } while (!panicMode_ && match(Lexer::TokenType::Separator, ","));
        }
        if (panicMode_) return e;
        expect(Lexer::TokenType::Separator, "]", "ожидался ']' после литерала массива");
        panicMode_ = false;
        e->span = spanFrom(first, previous());
        return e;
    }
    if (isNameLike(peek()) || isTypeNameLike(peek())) {
        auto path = parseNamePath();
        if (panicMode_) {
            auto e = std::make_unique<AST::NameExpr>();
            e->path = std::move(path);
            return e;
        }
        if (match(Lexer::TokenType::Separator, "{")) {
            auto e = std::make_unique<AST::StructLiteralExpr>();
            e->typePath = std::move(path);
            if (!check(Lexer::TokenType::Separator, "}")) {
                do {
                    if (panicMode_) break;
                    AST::StructFieldInit field;
                    const auto fieldFirst = peek();
                    field.name = expectIdentifierLike("ожидалось имя поля в литерале структуры");
                    if (panicMode_) break;
                    expect(Lexer::TokenType::Separator, ":", "ожидался ':' после имени поля");
                    if (panicMode_) break;
                    field.value = parseExpression();
                    if (panicMode_) break;
                    field.span = spanFrom(fieldFirst, previous());
                    e->fields.push_back(std::move(field));
                } while (!panicMode_ && match(Lexer::TokenType::Separator, ","));
            }
            if (panicMode_) return e;
            expect(Lexer::TokenType::Separator, "}", "ожидался '}' после литерала структуры");
            panicMode_ = false;
            e->span = spanFrom(first, previous());
            return e;
        }
        auto e = std::make_unique<AST::NameExpr>();
        e->path = std::move(path);
        e->span = spanFrom(first, previous());
        return e;
    }
    errorAt(peek(), "ожидалось выражение");
    panicMode_ = true;
    auto errorExpr = std::make_unique<AST::NameExpr>();
    errorExpr->path = {"<error>"};
    errorExpr->span = spanFrom(first, peek());
    return errorExpr;
}

bool Parser::isAssignable(const AST::Expr& expr) const {
    return dynamic_cast<const AST::NameExpr*>(&expr) != nullptr ||
           dynamic_cast<const AST::FieldExpr*>(&expr) != nullptr ||
           dynamic_cast<const AST::IndexExpr*>(&expr) != nullptr;
}

int Parser::precedenceOf(const Lexer::Token& tok) const {
    if (tok.type == Lexer::TokenType::Keyword && tok.lexeme == "as") return 7;
    if (tok.type != Lexer::TokenType::Operator) return 0;
    if (tok.lexeme == "||") return 1;
    if (tok.lexeme == "&&") return 2;
    if (tok.lexeme == "==" || tok.lexeme == "!=") return 3;
    if (tok.lexeme == "<" || tok.lexeme == "<=" || tok.lexeme == ">" || tok.lexeme == ">=") return 4;
    if (tok.lexeme == "+" || tok.lexeme == "-") return 5;
    if (tok.lexeme == "*" || tok.lexeme == "/" || tok.lexeme == "%") return 6;
    return 0;
}

bool Parser::isLeftAssociative(const Lexer::Token&) const {
    return true;
}

std::string formatDiagnostic(const Diagnostic& diagnostic) {
    return diagnostic.file + ":" + std::to_string(diagnostic.pos.line) + ":" +
           std::to_string(diagnostic.pos.column) + ": error: " + diagnostic.message;
}
}
