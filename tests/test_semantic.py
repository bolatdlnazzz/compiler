"""
Unit tests for the ZLang Semantic Analyzer.

Tests cover:
  * type inference for variable declarations
  * type checking for binary/unary operators
  * function call argument checking
  * return-type checking
  * undefined variable / function detection
  * duplicate declaration detection
  * int → float widening
"""
import pytest
from src.lexer import Lexer
from src.parser import Parser
from src.semantic import SemanticAnalyzer, SemanticError


def analyze(source: str) -> None:
    """Run the full front-end pipeline; raise SemanticError on violation."""
    tokens = Lexer(source).tokenize()
    ast = Parser(tokens).parse()
    SemanticAnalyzer().analyze(ast)


def ok(source: str) -> None:
    """Assert that *source* passes semantic analysis without errors."""
    analyze(source)  # must not raise


def bad(source: str) -> None:
    """Assert that *source* fails semantic analysis."""
    with pytest.raises(SemanticError):
        analyze(source)


# ── Type inference ────────────────────────────────────────────────────────

class TestTypeInference:
    def test_infer_int(self):
        ok("func f() -> void { let x = 42 }")

    def test_infer_float(self):
        ok("func f() -> void { let x = 3.14 }")

    def test_infer_bool(self):
        ok("func f() -> void { let x = true }")

    def test_infer_string(self):
        ok('func f() -> void { let x = "hi" }')

    def test_infer_from_func_call(self):
        ok("func inc(n: int) -> int { return n + 1 } func f() -> void { let x = inc(1) }")

    def test_explicit_type_matches(self):
        ok("func f() -> void { let x: int = 0 }")

    def test_explicit_type_mismatch(self):
        bad('func f() -> void { let x: int = "oops" }')

    def test_int_to_float_widening_in_decl(self):
        ok("func f() -> void { let x: float = 5 }")


# ── Variable scoping ──────────────────────────────────────────────────────

class TestScoping:
    def test_undefined_variable(self):
        bad("func f() -> void { print(x) }")

    def test_defined_variable(self):
        ok("func f() -> void { let x: int = 1  print(x) }")

    def test_duplicate_declaration_same_scope(self):
        bad("func f() -> void { let x: int = 1  let x: int = 2 }")

    def test_shadowing_in_nested_scope(self):
        ok("func f() -> void { let x: int = 1  if true { let x: int = 2  print(x) } }")

    def test_param_accessible_in_body(self):
        ok("func f(a: int) -> void { print(a) }")

    def test_for_var_scoped_to_body(self):
        # Using the loop variable outside the loop should fail
        bad("func f() -> void { for i in range(0, 3) { print(i) }  print(i) }")


# ── Assignment type checking ──────────────────────────────────────────────

class TestAssignment:
    def test_same_type_ok(self):
        ok("func f() -> void { let x: int = 0  x = 5 }")

    def test_int_to_float_widening(self):
        ok("func f() -> void { let x: float = 0.0  x = 5 }")

    def test_type_mismatch(self):
        bad('func f() -> void { let x: int = 0  x = "hi" }')

    def test_assign_to_undeclared(self):
        bad("func f() -> void { z = 1 }")


# ── Arithmetic operators ──────────────────────────────────────────────────

class TestArithmeticOps:
    def test_int_plus_int(self):
        ok("func f() -> void { let r = 1 + 2 }")

    def test_float_plus_float(self):
        ok("func f() -> void { let r = 1.0 + 2.0 }")

    def test_int_plus_float(self):
        ok("func f() -> void { let r = 1 + 2.0 }")

    def test_string_concat(self):
        ok('func f() -> void { let r = "a" + "b" }')

    def test_string_non_plus_fails(self):
        bad('func f() -> void { let r = "a" - "b" }')

    def test_bool_arithmetic_fails(self):
        bad("func f() -> void { let r = true + false }")


# ── Comparison operators ──────────────────────────────────────────────────

class TestComparisonOps:
    def test_int_comparison(self):
        ok("func f() -> void { let b = 1 < 2 }")

    def test_float_comparison(self):
        ok("func f() -> void { let b = 1.0 >= 2.0 }")

    def test_string_comparison_fails(self):
        bad('func f() -> void { let b = "a" < "b" }')


# ── Equality operators ────────────────────────────────────────────────────

class TestEqualityOps:
    def test_int_equality(self):
        ok("func f() -> void { let b = 1 == 1 }")

    def test_bool_equality(self):
        ok("func f() -> void { let b = true == false }")

    def test_type_mismatch_equality(self):
        bad('func f() -> void { let b = 1 == "x" }')


# ── Logical operators ─────────────────────────────────────────────────────

class TestLogicalOps:
    def test_and(self):
        ok("func f() -> void { let b = true and false }")

    def test_or(self):
        ok("func f() -> void { let b = true or false }")

    def test_and_with_non_bool_fails(self):
        bad("func f() -> void { let b = 1 and true }")

    def test_not(self):
        ok("func f() -> void { let b = not true }")

    def test_not_on_int_fails(self):
        bad("func f() -> void { let b = not 1 }")


# ── Unary minus ───────────────────────────────────────────────────────────

class TestUnaryMinus:
    def test_negate_int(self):
        ok("func f() -> void { let x = -5 }")

    def test_negate_float(self):
        ok("func f() -> void { let x = -1.5 }")

    def test_negate_bool_fails(self):
        bad("func f() -> void { let x = -true }")


# ── If / while conditions must be bool ────────────────────────────────────

class TestConditionTypes:
    def test_if_bool_ok(self):
        ok("func f() -> void { if true { print(1) } }")

    def test_if_int_fails(self):
        bad("func f() -> void { if 1 { print(1) } }")

    def test_while_bool_ok(self):
        ok("func f() -> void { let x: int = 0  while x < 10 { x = x + 1 } }")

    def test_while_int_fails(self):
        bad("func f() -> void { while 1 { print(1) } }")


# ── Return type checking ──────────────────────────────────────────────────

class TestReturnTypes:
    def test_correct_return_type(self):
        ok("func square(n: int) -> int { return n * n }")

    def test_wrong_return_type(self):
        bad('func square(n: int) -> int { return "oops" }')

    def test_void_function_return_no_value(self):
        ok("func f() -> void { return }")

    def test_void_function_return_value_fails(self):
        bad("func f() -> void { return 1 }")

    def test_int_to_float_widening_return(self):
        ok("func f() -> float { return 1 }")


# ── Function calls ────────────────────────────────────────────────────────

class TestFunctionCalls:
    _decl = "func add(a: int, b: int) -> int { return a + b } "

    def test_correct_call(self):
        ok(self._decl + "func f() -> void { let r = add(1, 2) }")

    def test_wrong_arg_count(self):
        bad(self._decl + "func f() -> void { let r = add(1) }")

    def test_wrong_arg_type(self):
        bad(self._decl + 'func f() -> void { let r = add(1, "x") }')

    def test_undefined_function(self):
        bad("func f() -> void { let r = nope(1) }")

    def test_int_to_float_widening_arg(self):
        ok("func square(x: float) -> float { return x * x } func f() -> void { let r = square(5) }")


# ── Duplicate function declaration ───────────────────────────────────────

class TestDuplicateFunc:
    def test_duplicate(self):
        bad("func f() -> void {} func f() -> void {}")


# ── For loop ─────────────────────────────────────────────────────────────

class TestForLoop:
    def test_valid_range(self):
        ok("func f() -> void { for i in range(0, 10) { print(i) } }")

    def test_non_numeric_range_fails(self):
        bad('func f() -> void { for i in range("a", "b") { print(i) } }')
