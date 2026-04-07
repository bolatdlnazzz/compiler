"""
Unit tests for the ZLang Parser.
"""
import pytest
from src.lexer import Lexer
from src.parser import Parser, ParseError
from src.ast_nodes import (
    Program, FuncDecl, Param, TypeNode, Block,
    VarDecl, Assignment, IfStmt, WhileStmt, ForStmt,
    ReturnStmt, PrintStmt, ExprStmt,
    BinOp, UnaryOp,
    IntLiteral, FloatLiteral, StringLiteral, BoolLiteral,
    Identifier, FuncCall,
)


def parse(source: str) -> Program:
    tokens = Lexer(source).tokenize()
    return Parser(tokens).parse()


def parse_func(source: str) -> FuncDecl:
    return parse(source).functions[0]


# ── Basic structure ───────────────────────────────────────────────────────

class TestProgram:
    def test_single_function(self):
        prog = parse("func main() -> void {}")
        assert len(prog.functions) == 1
        assert prog.functions[0].name == "main"

    def test_multiple_functions(self):
        prog = parse("func a() -> void {} func b() -> int { return 1 }")
        assert len(prog.functions) == 2

    def test_empty_source_raises(self):
        with pytest.raises(ParseError):
            parse("")


# ── Function declarations ─────────────────────────────────────────────────

class TestFuncDecl:
    def test_return_types(self):
        for type_kw in ("void", "int", "float", "bool", "string"):
            f = parse_func(f"func f() -> {type_kw} {{}}")
            assert f.return_type.name == type_kw

    def test_params(self):
        f = parse_func("func add(a: int, b: float) -> float {}")
        assert len(f.params) == 2
        assert f.params[0].name == "a"
        assert f.params[0].type.name == "int"
        assert f.params[1].name == "b"
        assert f.params[1].type.name == "float"

    def test_no_params(self):
        f = parse_func("func noop() -> void {}")
        assert f.params == []

    def test_missing_arrow_raises(self):
        with pytest.raises(ParseError):
            parse("func f() void {}")

    def test_missing_brace_raises(self):
        with pytest.raises(ParseError):
            parse("func f() -> void")


# ── Variable declarations ─────────────────────────────────────────────────

class TestVarDecl:
    def _body(self, stmt_src: str):
        f = parse_func(f"func f() -> void {{ {stmt_src} }}")
        return f.body.statements[0]

    def test_explicit_type(self):
        s = self._body("let x: int = 5")
        assert isinstance(s, VarDecl)
        assert s.name == "x"
        assert s.type_annotation.name == "int"
        assert isinstance(s.value, IntLiteral)

    def test_inferred_type(self):
        s = self._body("let y = 3.14")
        assert isinstance(s, VarDecl)
        assert s.type_annotation is None
        assert isinstance(s.value, FloatLiteral)

    def test_string_value(self):
        s = self._body('let s = "hello"')
        assert isinstance(s.value, StringLiteral)
        assert s.value.value == "hello"

    def test_bool_value(self):
        s = self._body("let b = true")
        assert isinstance(s.value, BoolLiteral)
        assert s.value.value is True


# ── Assignment ────────────────────────────────────────────────────────────

class TestAssignment:
    def test_simple(self):
        f = parse_func("func f() -> void { let x: int = 0  x = 1 }")
        stmt = f.body.statements[1]
        assert isinstance(stmt, Assignment)
        assert stmt.name == "x"
        assert isinstance(stmt.value, IntLiteral)


# ── If statement ──────────────────────────────────────────────────────────

class TestIfStmt:
    def _stmt(self, src: str) -> IfStmt:
        f = parse_func(f"func f() -> void {{ {src} }}")
        return f.body.statements[0]

    def test_if_only(self):
        s = self._stmt("if true { print(1) }")
        assert isinstance(s, IfStmt)
        assert s.else_block is None

    def test_if_else(self):
        s = self._stmt("if true { print(1) } else { print(2) }")
        assert s.else_block is not None

    def test_condition_is_expression(self):
        s = self._stmt("if x == 1 { print(x) }")
        assert isinstance(s.condition, BinOp)
        assert s.condition.op == "=="


# ── While loop ────────────────────────────────────────────────────────────

class TestWhileStmt:
    def test_while(self):
        f = parse_func("func f() -> void { while x < 10 { x = x + 1 } }")
        s = f.body.statements[0]
        assert isinstance(s, WhileStmt)
        assert isinstance(s.condition, BinOp)


# ── For loop ─────────────────────────────────────────────────────────────

class TestForStmt:
    def test_for(self):
        f = parse_func("func f() -> void { for i in range(0, 10) { print(i) } }")
        s = f.body.statements[0]
        assert isinstance(s, ForStmt)
        assert s.var == "i"
        assert isinstance(s.start, IntLiteral)
        assert isinstance(s.end, IntLiteral)


# ── Return statement ──────────────────────────────────────────────────────

class TestReturnStmt:
    def test_return_value(self):
        f = parse_func("func f() -> int { return 42 }")
        s = f.body.statements[0]
        assert isinstance(s, ReturnStmt)
        assert isinstance(s.value, IntLiteral)

    def test_return_void(self):
        f = parse_func("func f() -> void { return }")
        s = f.body.statements[0]
        assert isinstance(s, ReturnStmt)
        assert s.value is None


# ── Print statement ───────────────────────────────────────────────────────

class TestPrintStmt:
    def test_print(self):
        f = parse_func('func f() -> void { print("hi") }')
        s = f.body.statements[0]
        assert isinstance(s, PrintStmt)
        assert isinstance(s.value, StringLiteral)


# ── Expression statements (function calls as statements) ──────────────────

class TestExprStmt:
    def test_func_call_as_stmt(self):
        prog = parse("func g() -> void {} func f() -> void { g() }")
        s = prog.functions[1].body.statements[0]
        assert isinstance(s, ExprStmt)
        assert isinstance(s.expr, FuncCall)
        assert s.expr.name == "g"


# ── Expressions ──────────────────────────────────────────────────────────

class TestExpressions:
    def _expr(self, expr_src: str):
        f = parse_func(f"func f() -> void {{ let _x = {expr_src} }}")
        return f.body.statements[0].value

    def test_int_literal(self):
        e = self._expr("7")
        assert isinstance(e, IntLiteral)
        assert e.value == 7

    def test_float_literal(self):
        e = self._expr("2.5")
        assert isinstance(e, FloatLiteral)
        assert e.value == 2.5

    def test_bool_literal_false(self):
        e = self._expr("false")
        assert isinstance(e, BoolLiteral)
        assert e.value is False

    def test_string_literal(self):
        e = self._expr('"world"')
        assert isinstance(e, StringLiteral)
        assert e.value == "world"

    def test_identifier(self):
        e = self._expr("myVar")
        assert isinstance(e, Identifier)
        assert e.name == "myVar"

    def test_addition(self):
        e = self._expr("1 + 2")
        assert isinstance(e, BinOp)
        assert e.op == "+"

    def test_precedence_mul_over_add(self):
        # 1 + 2 * 3  →  BinOp('+', 1, BinOp('*', 2, 3))
        e = self._expr("1 + 2 * 3")
        assert isinstance(e, BinOp)
        assert e.op == "+"
        assert isinstance(e.right, BinOp)
        assert e.right.op == "*"

    def test_parentheses_override_precedence(self):
        # (1 + 2) * 3  →  BinOp('*', BinOp('+', 1, 2), 3)
        e = self._expr("(1 + 2) * 3")
        assert isinstance(e, BinOp)
        assert e.op == "*"
        assert isinstance(e.left, BinOp)
        assert e.left.op == "+"

    def test_unary_minus(self):
        e = self._expr("-5")
        assert isinstance(e, UnaryOp)
        assert e.op == "-"

    def test_unary_not(self):
        e = self._expr("not true")
        assert isinstance(e, UnaryOp)
        assert e.op == "not"

    def test_logical_and(self):
        e = self._expr("a and b")
        assert isinstance(e, BinOp)
        assert e.op == "and"

    def test_logical_or(self):
        e = self._expr("a or b")
        assert isinstance(e, BinOp)
        assert e.op == "or"

    def test_comparison(self):
        e = self._expr("x >= 0")
        assert isinstance(e, BinOp)
        assert e.op == ">="

    def test_equality(self):
        e = self._expr("a == b")
        assert isinstance(e, BinOp)
        assert e.op == "=="

    def test_func_call_expr(self):
        e = self._expr("foo(1, 2)")
        assert isinstance(e, FuncCall)
        assert e.name == "foo"
        assert len(e.args) == 2

    def test_func_call_no_args(self):
        e = self._expr("bar()")
        assert isinstance(e, FuncCall)
        assert e.args == []
