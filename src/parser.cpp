using namespace std::literals;

AST::DeclPtr Parser::parseDeclaration() {
    if (match(Lexer::TokenType::Keyword, "namespace")) return parseNamespaceDecl();
    if (match(Lexer::TokenType::Keyword, "type"))      return parseTypeAliasDecl();
    if (match(Lexer::TokenType::Keyword, "struct"))    return parseStructDecl();
    if (match(Lexer::TokenType::Keyword, "fn"))        return parseFunctionDecl();

    errorAt(peek(), "ожидалось верхнеуровневое объявление: namespace, type, struct или fn");
}

std::unique_ptr<AST::FunctionDecl> Parser::parseFunctionDecl() {
    const auto first = previous(); // "fn" уже съеден match()

    auto node = std::make_unique<AST::FunctionDecl>();
    node->name = expectIdentifierLike("ожидалось имя функции");

    expect(Lexer::TokenType::Separator, "(", "ожидался '(' после имени функции");

    if (!check(Lexer::TokenType::Separator, ")")) {
        do {
            AST::Param param;
            const auto& paramTok = peek();
            param.name = expectIdentifierLike("ожидалось имя параметра");
            expect(Lexer::TokenType::Separator, ":", "ожидался ':' после имени параметра");
            param.type = parseTypeExpr();
            param.span = spanFrom(paramTok, previous());
            node->params.push_back(std::move(param));
        } while (match(Lexer::TokenType::Separator, ","));
    }

    expect(Lexer::TokenType::Separator, ")", "ожидался ')' после списка параметров");
    expect(Lexer::TokenType::Separator, "->", "ожидался '->' перед типом результата");
    node->returnType = parseTypeExpr();
    node->body = parseBlock();
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
        auto s = std::make_unique<AST::BreakStmt>();
        expect(Lexer::TokenType::Separator, ";", "ожидался ';' после break");
        s->span = spanFrom(previous(), previous());
        return s;
    }
    if (match(Lexer::TokenType::Keyword, "continue")) {
        auto s = std::make_unique<AST::ContinueStmt>();
        expect(Lexer::TokenType::Separator, ";", "ожидался ';' после continue");
        s->span = spanFrom(previous(), previous());
        return s;
    }
    if (match(Lexer::TokenType::Keyword, "return"))   return parseReturnStmt();
    if (check(Lexer::TokenType::Separator, "{"))      return parseBlock();

    // По умолчанию: либо expr_stmt, либо assign_stmt
    const auto first = peek();
    auto lhsOrExpr = parseExpression();

    if (match(Lexer::TokenType::Operator, "=")) {
        if (!isAssignable(*lhsOrExpr)) {
            errorAt(previous(), "левая часть присваивания должна быть lvalue");
        }
        auto value = parseExpression();
        expect(Lexer::TokenType::Separator, ";", "ожидался ';' после присваивания");

        auto s = std::make_unique<AST::AssignStmt>();
        s->target = std::move(lhsOrExpr);
        s->value  = std::move(value);
        s->span   = spanFrom(first, previous());
        return s;
    }

    expect(Lexer::TokenType::Separator, ";", "ожидался ';' после выражения");
    auto s = std::make_unique<AST::ExprStmt>();
    s->expr = std::move(lhsOrExpr);
    s->span = spanFrom(first, previous());
    return s;
}
