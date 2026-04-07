"""
Semantic Analyzer for ZLang.

Performs two passes over the AST:

Pass 1 – collect all function signatures (name → (param_types, return_type))
         so that mutual/forward calls type-check correctly.

Pass 2 – walk every statement and expression:
  * resolve variable / parameter types from the symbol table,
  * infer the type of every expression node (stored in resolved_type),
  * type-check operands and return statements,
  * detect undefined variables and functions, duplicate declarations, etc.

Type rules (summary)
────────────────────
  int  op int   → int        (arithmetic)
  float op float → float     (arithmetic)
  int  op float / float op int → float  (numeric promotion)
  string + string → string   (concatenation)
  any   == / != any (same type) → bool
  numeric < > <= >= numeric  → bool
  bool and/or bool → bool
  not bool → bool
  - int → int,  - float → float
"""
from typing import Dict, List, Optional, Tuple
from .ast_nodes import (
    ASTNode, Program, FuncDecl, Param, TypeNode, Block,
    VarDecl, Assignment, IfStmt, WhileStmt, ForStmt,
    ReturnStmt, PrintStmt, ExprStmt,
    BinOp, UnaryOp,
    IntLiteral, FloatLiteral, StringLiteral, BoolLiteral,
    Identifier, FuncCall,
)


# ---------------------------------------------------------------------------
# Error type
# ---------------------------------------------------------------------------

class SemanticError(Exception):
    def __init__(self, message: str, line: int = 0, col: int = 0) -> None:
        super().__init__(f"Semantic error at line {line}, col {col}: {message}")
        self.line = line
        self.col = col


# ---------------------------------------------------------------------------
# Helper: scope stack (symbol table)
# ---------------------------------------------------------------------------

class ScopeStack:
    """Stack of dicts mapping name → type string."""

    def __init__(self) -> None:
        self._stack: List[Dict[str, str]] = []

    def push(self) -> None:
        self._stack.append({})

    def pop(self) -> None:
        self._stack.pop()

    def define(self, name: str, type_: str, line: int, col: int) -> None:
        if name in self._stack[-1]:
            raise SemanticError(f"Variable '{name}' already declared in this scope", line, col)
        self._stack[-1][name] = type_

    def lookup(self, name: str) -> Optional[str]:
        for scope in reversed(self._stack):
            if name in scope:
                return scope[name]
        return None

    def assign_check(self, name: str, line: int, col: int) -> str:
        """Return the type of an already-declared variable or raise."""
        t = self.lookup(name)
        if t is None:
            raise SemanticError(f"Assignment to undeclared variable '{name}'", line, col)
        return t


# ---------------------------------------------------------------------------
# Type arithmetic helpers
# ---------------------------------------------------------------------------

NUMERIC = {"int", "float"}


def _numeric_result(t1: str, t2: str) -> str:
    """Return the result type for a numeric binary operation."""
    if t1 == "float" or t2 == "float":
        return "float"
    return "int"


# ---------------------------------------------------------------------------
# Semantic Analyzer
# ---------------------------------------------------------------------------

class SemanticAnalyzer:
    """
    Walks the AST, resolves types, annotates expression nodes with
    ``resolved_type``, and raises ``SemanticError`` on any violation.
    """

    def __init__(self) -> None:
        # function name → ([(param_name, param_type)], return_type)
        self._functions: Dict[str, Tuple[List[Tuple[str, str]], str]] = {}
        self._scopes = ScopeStack()
        self._current_return_type: Optional[str] = None

    # ── Public entry point ────────────────────────────────────────────────

    def analyze(self, program: Program) -> None:
        """Run both passes over the program AST."""
        self._collect_signatures(program)
        for func in program.functions:
            self._check_func(func)

    # ── Pass 1: collect function signatures ───────────────────────────────

    def _collect_signatures(self, program: Program) -> None:
        for func in program.functions:
            if func.name in self._functions:
                raise SemanticError(
                    f"Duplicate function declaration '{func.name}'",
                    func.line, func.col,
                )
            params = [(p.name, p.type.name) for p in func.params]
            self._functions[func.name] = (params, func.return_type.name)

    # ── Pass 2: type-check each function ──────────────────────────────────

    def _check_func(self, func: FuncDecl) -> None:
        self._current_return_type = func.return_type.name
        self._scopes.push()
        # Add parameters to the innermost scope
        for p in func.params:
            self._scopes.define(p.name, p.type.name, p.line, p.col)
        self._check_block(func.body)
        self._scopes.pop()
        self._current_return_type = None

    def _check_block(self, block: Block) -> None:
        self._scopes.push()
        for stmt in block.statements:
            self._check_stmt(stmt)
        self._scopes.pop()

    # ── Statements ────────────────────────────────────────────────────────

    def _check_stmt(self, stmt) -> None:
        if isinstance(stmt, VarDecl):
            self._check_var_decl(stmt)
        elif isinstance(stmt, Assignment):
            self._check_assignment(stmt)
        elif isinstance(stmt, IfStmt):
            self._check_if(stmt)
        elif isinstance(stmt, WhileStmt):
            self._check_while(stmt)
        elif isinstance(stmt, ForStmt):
            self._check_for(stmt)
        elif isinstance(stmt, ReturnStmt):
            self._check_return(stmt)
        elif isinstance(stmt, PrintStmt):
            self._infer_expr(stmt.value)  # any type is printable
        elif isinstance(stmt, ExprStmt):
            self._infer_expr(stmt.expr)
        else:
            raise SemanticError(f"Unknown statement type: {type(stmt).__name__}", stmt.line, stmt.col)

    def _check_var_decl(self, stmt: VarDecl) -> None:
        inferred = self._infer_expr(stmt.value)
        if stmt.type_annotation is not None:
            declared = stmt.type_annotation.name
            # Allow int literal assigned to float variable (widening)
            if declared != inferred:
                if declared == "float" and inferred == "int":
                    pass  # widening OK
                else:
                    raise SemanticError(
                        f"Type mismatch in declaration of '{stmt.name}': "
                        f"declared '{declared}', got '{inferred}'",
                        stmt.line, stmt.col,
                    )
            self._scopes.define(stmt.name, declared, stmt.line, stmt.col)
        else:
            # Type inference
            self._scopes.define(stmt.name, inferred, stmt.line, stmt.col)

    def _check_assignment(self, stmt: Assignment) -> None:
        existing_type = self._scopes.assign_check(stmt.name, stmt.line, stmt.col)
        rhs_type = self._infer_expr(stmt.value)
        if existing_type != rhs_type:
            # Allow int → float widening
            if existing_type == "float" and rhs_type == "int":
                return
            raise SemanticError(
                f"Cannot assign '{rhs_type}' to variable '{stmt.name}' of type '{existing_type}'",
                stmt.line, stmt.col,
            )

    def _check_if(self, stmt: IfStmt) -> None:
        cond_type = self._infer_expr(stmt.condition)
        if cond_type != "bool":
            raise SemanticError(
                f"'if' condition must be bool, got '{cond_type}'",
                stmt.line, stmt.col,
            )
        self._check_block(stmt.then_block)
        if stmt.else_block is not None:
            self._check_block(stmt.else_block)

    def _check_while(self, stmt: WhileStmt) -> None:
        cond_type = self._infer_expr(stmt.condition)
        if cond_type != "bool":
            raise SemanticError(
                f"'while' condition must be bool, got '{cond_type}'",
                stmt.line, stmt.col,
            )
        self._check_block(stmt.body)

    def _check_for(self, stmt: ForStmt) -> None:
        start_type = self._infer_expr(stmt.start)
        end_type = self._infer_expr(stmt.end)
        if start_type not in NUMERIC or end_type not in NUMERIC:
            raise SemanticError(
                f"'for' range bounds must be numeric, got '{start_type}' and '{end_type}'",
                stmt.line, stmt.col,
            )
        # The loop variable is an int introduced inside a fresh scope
        self._scopes.push()
        self._scopes.define(stmt.var, "int", stmt.line, stmt.col)
        # check body without creating another scope (block opens its own)
        for s in stmt.body.statements:
            self._check_stmt(s)
        self._scopes.pop()

    def _check_return(self, stmt: ReturnStmt) -> None:
        if self._current_return_type is None:
            raise SemanticError("'return' outside function", stmt.line, stmt.col)
        if stmt.value is None:
            if self._current_return_type != "void":
                raise SemanticError(
                    f"Function must return '{self._current_return_type}', not void",
                    stmt.line, stmt.col,
                )
            return
        ret_type = self._infer_expr(stmt.value)
        expected = self._current_return_type
        if ret_type != expected:
            if expected == "float" and ret_type == "int":
                return  # widening OK
            raise SemanticError(
                f"Return type mismatch: expected '{expected}', got '{ret_type}'",
                stmt.line, stmt.col,
            )

    # ── Expression type inference ─────────────────────────────────────────

    def _infer_expr(self, node) -> str:
        """Return the type string for *node* and annotate node.resolved_type."""
        t = self._infer_expr_inner(node)
        if hasattr(node, "resolved_type"):
            node.resolved_type = t
        return t

    def _infer_expr_inner(self, node) -> str:
        if isinstance(node, IntLiteral):
            return "int"
        if isinstance(node, FloatLiteral):
            return "float"
        if isinstance(node, StringLiteral):
            return "string"
        if isinstance(node, BoolLiteral):
            return "bool"

        if isinstance(node, Identifier):
            t = self._scopes.lookup(node.name)
            if t is None:
                raise SemanticError(
                    f"Undefined variable '{node.name}'", node.line, node.col
                )
            return t

        if isinstance(node, FuncCall):
            return self._infer_func_call(node)

        if isinstance(node, UnaryOp):
            return self._infer_unary(node)

        if isinstance(node, BinOp):
            return self._infer_binop(node)

        raise SemanticError(f"Unknown expression type: {type(node).__name__}", node.line, node.col)

    def _infer_func_call(self, node: FuncCall) -> str:
        if node.name not in self._functions:
            raise SemanticError(
                f"Call to undefined function '{node.name}'", node.line, node.col
            )
        params, ret_type = self._functions[node.name]
        if len(node.args) != len(params):
            raise SemanticError(
                f"Function '{node.name}' expects {len(params)} argument(s), "
                f"got {len(node.args)}",
                node.line, node.col,
            )
        for arg, (pname, ptype) in zip(node.args, params):
            arg_type = self._infer_expr(arg)
            if arg_type != ptype:
                # Allow int → float widening
                if ptype == "float" and arg_type == "int":
                    continue
                raise SemanticError(
                    f"Argument type mismatch in call to '{node.name}': "
                    f"parameter '{pname}' is '{ptype}', got '{arg_type}'",
                    node.line, node.col,
                )
        return ret_type

    def _infer_unary(self, node: UnaryOp) -> str:
        t = self._infer_expr(node.operand)
        if node.op == "not":
            if t != "bool":
                raise SemanticError(
                    f"Operator 'not' requires bool, got '{t}'", node.line, node.col
                )
            return "bool"
        if node.op == "-":
            if t not in NUMERIC:
                raise SemanticError(
                    f"Unary '-' requires numeric type, got '{t}'", node.line, node.col
                )
            return t
        raise SemanticError(f"Unknown unary operator '{node.op}'", node.line, node.col)

    def _infer_binop(self, node: BinOp) -> str:
        lt = self._infer_expr(node.left)
        rt = self._infer_expr(node.right)

        op = node.op

        # Logical operators
        if op in ("and", "or"):
            if lt != "bool" or rt != "bool":
                raise SemanticError(
                    f"Operator '{op}' requires bool operands, got '{lt}' and '{rt}'",
                    node.line, node.col,
                )
            return "bool"

        # Equality / inequality – both sides must have the same type
        if op in ("==", "!="):
            if lt != rt:
                # Allow numeric cross-comparison
                if lt in NUMERIC and rt in NUMERIC:
                    return "bool"
                raise SemanticError(
                    f"Type mismatch for '{op}': '{lt}' vs '{rt}'",
                    node.line, node.col,
                )
            return "bool"

        # Ordering comparisons
        if op in ("<", ">", "<=", ">="):
            if lt not in NUMERIC or rt not in NUMERIC:
                raise SemanticError(
                    f"Operator '{op}' requires numeric operands, got '{lt}' and '{rt}'",
                    node.line, node.col,
                )
            return "bool"

        # Arithmetic
        if op in ("+", "-", "*", "/", "%"):
            # String concatenation
            if op == "+" and lt == "string" and rt == "string":
                return "string"
            if lt not in NUMERIC or rt not in NUMERIC:
                raise SemanticError(
                    f"Operator '{op}' requires numeric operands, got '{lt}' and '{rt}'",
                    node.line, node.col,
                )
            return _numeric_result(lt, rt)

        raise SemanticError(f"Unknown binary operator '{op}'", node.line, node.col)
