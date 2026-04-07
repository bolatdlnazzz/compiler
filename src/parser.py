"""
Recursive-Descent Parser for ZLang.

Consumes a token stream (produced by the Lexer) and builds an Abstract
Syntax Tree.

Full grammar (EBNF notation)
─────────────────────────────────────────────────────────────────────────────
program     = func_decl { func_decl } ;

func_decl   = 'func' IDENT '(' param_list ')' '->' type block ;
param_list  = [ param { ',' param } ] ;
param       = IDENT ':' type ;
type        = 'int' | 'float' | 'bool' | 'string' | 'void' ;

block       = '{' { stmt } '}' ;

stmt        = var_decl
            | assign_stmt
            | if_stmt
            | while_stmt
            | for_stmt
            | return_stmt
            | print_stmt
            | expr_stmt ;

var_decl    = 'let' IDENT [ ':' type ] '=' expr ;
assign_stmt = IDENT '=' expr ;
if_stmt     = 'if' expr block [ 'else' block ] ;
while_stmt  = 'while' expr block ;
for_stmt    = 'for' IDENT 'in' 'range' '(' expr ',' expr ')' block ;
return_stmt = 'return' [ expr ] ;
print_stmt  = 'print' '(' expr ')' ;
expr_stmt   = expr ;

expr        = or_expr ;
or_expr     = and_expr  { 'or'  and_expr  } ;
and_expr    = eq_expr   { 'and' eq_expr   } ;
eq_expr     = cmp_expr  { ( '==' | '!=' ) cmp_expr  } ;
cmp_expr    = add_expr  { ( '<' | '>' | '<=' | '>=' ) add_expr  } ;
add_expr    = mul_expr  { ( '+' | '-' ) mul_expr  } ;
mul_expr    = unary     { ( '*' | '/' | '%' ) unary } ;
unary       = ( 'not' | '!' | '-' ) unary | primary ;
primary     = INTEGER | FLOAT | STRING | BOOL
            | IDENT [ '(' arg_list ')' ]
            | '(' expr ')' ;
arg_list    = [ expr { ',' expr } ] ;
─────────────────────────────────────────────────────────────────────────────
"""
from typing import List, Optional
from .lexer import Token, TokenType
from .ast_nodes import (
    Program, FuncDecl, Param, TypeNode, Block,
    VarDecl, Assignment, IfStmt, WhileStmt, ForStmt,
    ReturnStmt, PrintStmt, ExprStmt,
    BinOp, UnaryOp,
    IntLiteral, FloatLiteral, StringLiteral, BoolLiteral,
    Identifier, FuncCall,
)


class ParseError(Exception):
    def __init__(self, message: str, line: int, col: int) -> None:
        super().__init__(f"Parse error at line {line}, col {col}: {message}")
        self.line = line
        self.col = col


class Parser:
    """Builds an AST from the token list returned by ``Lexer.tokenize()``."""

    def __init__(self, tokens: List[Token]) -> None:
        self.tokens = tokens
        self.pos = 0

    # ── Helpers ──────────────────────────────────────────────────────────────

    def _cur(self) -> Token:
        return self.tokens[self.pos]

    def _peek_ahead(self, offset: int = 1) -> Token:
        idx = self.pos + offset
        return self.tokens[min(idx, len(self.tokens) - 1)]

    def _advance(self) -> Token:
        tok = self.tokens[self.pos]
        if self.pos < len(self.tokens) - 1:
            self.pos += 1
        return tok

    def _check(self, *types: TokenType) -> bool:
        return self._cur().type in types

    def _match(self, *types: TokenType) -> Optional[Token]:
        if self._check(*types):
            return self._advance()
        return None

    def _expect(self, ttype: TokenType, msg: str = "") -> Token:
        if self._check(ttype):
            return self._advance()
        cur = self._cur()
        raise ParseError(
            msg or f"Expected {ttype.name}, got {cur.type.name} ({cur.value!r})",
            cur.line, cur.col,
        )

    def _error(self, msg: str) -> None:
        cur = self._cur()
        raise ParseError(msg, cur.line, cur.col)

    # ── Grammar: top level ────────────────────────────────────────────────

    def parse(self) -> Program:
        """Parse the token stream and return the root Program node."""
        line, col = self._cur().line, self._cur().col
        funcs: List[FuncDecl] = []
        while not self._check(TokenType.EOF):
            funcs.append(self._parse_func_decl())
        if not funcs:
            self._error("A ZLang program must contain at least one function")
        return Program(line=line, col=col, functions=funcs)

    # ── Types ─────────────────────────────────────────────────────────────

    def _parse_type(self) -> TypeNode:
        _type_map = {
            TokenType.TYPE_INT:    "int",
            TokenType.TYPE_FLOAT:  "float",
            TokenType.TYPE_BOOL:   "bool",
            TokenType.TYPE_STRING: "string",
            TokenType.TYPE_VOID:   "void",
        }
        cur = self._cur()
        if cur.type in _type_map:
            self._advance()
            return TypeNode(name=_type_map[cur.type], line=cur.line, col=cur.col)
        self._error(
            f"Expected a type name (int/float/bool/string/void), "
            f"got {cur.type.name} ({cur.value!r})"
        )

    # ── Function declaration ───────────────────────────────────────────────

    def _parse_func_decl(self) -> FuncDecl:
        tok = self._expect(TokenType.FUNC, "Expected 'func' keyword")
        name_tok = self._expect(TokenType.IDENT, "Expected function name after 'func'")
        self._expect(TokenType.LPAREN, "Expected '(' after function name")
        params = self._parse_param_list()
        self._expect(TokenType.RPAREN, "Expected ')' after parameter list")
        self._expect(TokenType.ARROW, "Expected '->' before return type")
        ret_type = self._parse_type()
        body = self._parse_block()
        return FuncDecl(
            name=name_tok.value,
            params=params,
            return_type=ret_type,
            body=body,
            line=tok.line,
            col=tok.col,
        )

    def _parse_param_list(self) -> List[Param]:
        params: List[Param] = []
        if self._check(TokenType.RPAREN):
            return params
        params.append(self._parse_param())
        while self._match(TokenType.COMMA):
            params.append(self._parse_param())
        return params

    def _parse_param(self) -> Param:
        name_tok = self._expect(TokenType.IDENT, "Expected parameter name")
        self._expect(TokenType.COLON, "Expected ':' after parameter name")
        ptype = self._parse_type()
        return Param(name=name_tok.value, type=ptype, line=name_tok.line, col=name_tok.col)

    # ── Block ─────────────────────────────────────────────────────────────

    def _parse_block(self) -> Block:
        tok = self._expect(TokenType.LBRACE, "Expected '{'")
        stmts = []
        while not self._check(TokenType.RBRACE) and not self._check(TokenType.EOF):
            stmts.append(self._parse_stmt())
        self._expect(TokenType.RBRACE, "Expected '}'")
        return Block(statements=stmts, line=tok.line, col=tok.col)

    # ── Statements ────────────────────────────────────────────────────────

    def _parse_stmt(self):
        cur = self._cur()

        if cur.type == TokenType.LET:
            return self._parse_var_decl()
        if cur.type == TokenType.IF:
            return self._parse_if_stmt()
        if cur.type == TokenType.WHILE:
            return self._parse_while_stmt()
        if cur.type == TokenType.FOR:
            return self._parse_for_stmt()
        if cur.type == TokenType.RETURN:
            return self._parse_return_stmt()
        if cur.type == TokenType.PRINT:
            return self._parse_print_stmt()
        # Assignment vs expression statement: IDENT '=' …
        if (
            cur.type == TokenType.IDENT
            and self._peek_ahead().type == TokenType.ASSIGN
        ):
            return self._parse_assignment()
        return self._parse_expr_stmt()

    def _parse_var_decl(self) -> VarDecl:
        tok = self._expect(TokenType.LET)
        name_tok = self._expect(TokenType.IDENT, "Expected variable name after 'let'")
        ann: Optional[TypeNode] = None
        if self._match(TokenType.COLON):
            ann = self._parse_type()
        self._expect(TokenType.ASSIGN, "Expected '=' in variable declaration")
        value = self._parse_expr()
        return VarDecl(
            name=name_tok.value,
            type_annotation=ann,
            value=value,
            line=tok.line,
            col=tok.col,
        )

    def _parse_assignment(self) -> Assignment:
        name_tok = self._expect(TokenType.IDENT)
        self._expect(TokenType.ASSIGN)
        value = self._parse_expr()
        return Assignment(name=name_tok.value, value=value, line=name_tok.line, col=name_tok.col)

    def _parse_if_stmt(self) -> IfStmt:
        tok = self._expect(TokenType.IF)
        cond = self._parse_expr()
        then_blk = self._parse_block()
        else_blk: Optional[Block] = None
        if self._match(TokenType.ELSE):
            else_blk = self._parse_block()
        return IfStmt(condition=cond, then_block=then_blk, else_block=else_blk, line=tok.line, col=tok.col)

    def _parse_while_stmt(self) -> WhileStmt:
        tok = self._expect(TokenType.WHILE)
        cond = self._parse_expr()
        body = self._parse_block()
        return WhileStmt(condition=cond, body=body, line=tok.line, col=tok.col)

    def _parse_for_stmt(self) -> ForStmt:
        tok = self._expect(TokenType.FOR)
        var_tok = self._expect(TokenType.IDENT, "Expected loop variable after 'for'")
        self._expect(TokenType.IN, "Expected 'in' after loop variable")
        self._expect(TokenType.RANGE, "Expected 'range' after 'in'")
        self._expect(TokenType.LPAREN, "Expected '(' after 'range'")
        start = self._parse_expr()
        self._expect(TokenType.COMMA, "Expected ',' between range bounds")
        end = self._parse_expr()
        self._expect(TokenType.RPAREN, "Expected ')' after range bounds")
        body = self._parse_block()
        return ForStmt(var=var_tok.value, start=start, end=end, body=body, line=tok.line, col=tok.col)

    def _parse_return_stmt(self) -> ReturnStmt:
        tok = self._expect(TokenType.RETURN)
        # If the next token can start an expression, parse it
        if self._check(TokenType.RBRACE) or self._check(TokenType.EOF):
            return ReturnStmt(value=None, line=tok.line, col=tok.col)
        return ReturnStmt(value=self._parse_expr(), line=tok.line, col=tok.col)

    def _parse_print_stmt(self) -> PrintStmt:
        tok = self._expect(TokenType.PRINT)
        self._expect(TokenType.LPAREN, "Expected '(' after 'print'")
        value = self._parse_expr()
        self._expect(TokenType.RPAREN, "Expected ')' after print argument")
        return PrintStmt(value=value, line=tok.line, col=tok.col)

    def _parse_expr_stmt(self) -> ExprStmt:
        tok = self._cur()
        expr = self._parse_expr()
        return ExprStmt(expr=expr, line=tok.line, col=tok.col)

    # ── Expressions (precedence climbing via separate methods) ────────────

    def _parse_expr(self):
        return self._parse_or()

    def _parse_or(self):
        left = self._parse_and()
        while self._check(TokenType.OR):
            op_tok = self._advance()
            right = self._parse_and()
            left = BinOp(op="or", left=left, right=right, line=op_tok.line, col=op_tok.col)
        return left

    def _parse_and(self):
        left = self._parse_equality()
        while self._check(TokenType.AND):
            op_tok = self._advance()
            right = self._parse_equality()
            left = BinOp(op="and", left=left, right=right, line=op_tok.line, col=op_tok.col)
        return left

    def _parse_equality(self):
        left = self._parse_comparison()
        while self._check(TokenType.EQ, TokenType.NEQ):
            op_tok = self._advance()
            right = self._parse_comparison()
            left = BinOp(op=op_tok.value, left=left, right=right, line=op_tok.line, col=op_tok.col)
        return left

    def _parse_comparison(self):
        left = self._parse_addition()
        while self._check(TokenType.LT, TokenType.GT, TokenType.LTE, TokenType.GTE):
            op_tok = self._advance()
            right = self._parse_addition()
            left = BinOp(op=op_tok.value, left=left, right=right, line=op_tok.line, col=op_tok.col)
        return left

    def _parse_addition(self):
        left = self._parse_multiplication()
        while self._check(TokenType.PLUS, TokenType.MINUS):
            op_tok = self._advance()
            right = self._parse_multiplication()
            left = BinOp(op=op_tok.value, left=left, right=right, line=op_tok.line, col=op_tok.col)
        return left

    def _parse_multiplication(self):
        left = self._parse_unary()
        while self._check(TokenType.STAR, TokenType.SLASH, TokenType.PERCENT):
            op_tok = self._advance()
            right = self._parse_unary()
            left = BinOp(op=op_tok.value, left=left, right=right, line=op_tok.line, col=op_tok.col)
        return left

    def _parse_unary(self):
        cur = self._cur()
        if cur.type in (TokenType.NOT, TokenType.MINUS):
            op_tok = self._advance()
            operand = self._parse_unary()
            return UnaryOp(op=op_tok.value if op_tok.type == TokenType.MINUS else "not",
                           operand=operand, line=op_tok.line, col=op_tok.col)
        return self._parse_primary()

    def _parse_primary(self):
        cur = self._cur()

        if cur.type == TokenType.INTEGER:
            self._advance()
            return IntLiteral(value=cur.value, line=cur.line, col=cur.col)

        if cur.type == TokenType.FLOAT:
            self._advance()
            return FloatLiteral(value=cur.value, line=cur.line, col=cur.col)

        if cur.type == TokenType.STRING:
            self._advance()
            return StringLiteral(value=cur.value, line=cur.line, col=cur.col)

        if cur.type == TokenType.BOOL:
            self._advance()
            return BoolLiteral(value=cur.value, line=cur.line, col=cur.col)

        if cur.type == TokenType.IDENT:
            self._advance()
            # function call?
            if self._check(TokenType.LPAREN):
                self._advance()
                args = self._parse_arg_list()
                self._expect(TokenType.RPAREN, "Expected ')' after argument list")
                return FuncCall(name=cur.value, args=args, line=cur.line, col=cur.col)
            return Identifier(name=cur.value, line=cur.line, col=cur.col)

        if cur.type == TokenType.LPAREN:
            self._advance()
            expr = self._parse_expr()
            self._expect(TokenType.RPAREN, "Expected ')' after grouped expression")
            return expr

        self._error(
            f"Unexpected token in expression: {cur.type.name} ({cur.value!r})"
        )

    def _parse_arg_list(self) -> list:
        args = []
        if self._check(TokenType.RPAREN):
            return args
        args.append(self._parse_expr())
        while self._match(TokenType.COMMA):
            args.append(self._parse_expr())
        return args
