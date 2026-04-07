"""
Abstract Syntax Tree (AST) Node Definitions for ZLang.
Each node represents a syntactic construct in the language grammar.
"""
from dataclasses import dataclass, field
from typing import Any, List, Optional


# ---------------------------------------------------------------------------
# Base node
# ---------------------------------------------------------------------------

@dataclass
class ASTNode:
    """Base class for all AST nodes; carries source-location info."""
    line: int = 0
    col: int = 0


# ---------------------------------------------------------------------------
# Type representation
# ---------------------------------------------------------------------------

@dataclass
class TypeNode(ASTNode):
    """Represents a type annotation: int | float | bool | string | void."""
    name: str = ""


# ---------------------------------------------------------------------------
# Top-level program
# ---------------------------------------------------------------------------

@dataclass
class Program(ASTNode):
    """Root node: an ordered list of function declarations."""
    functions: List["FuncDecl"] = field(default_factory=list)


# ---------------------------------------------------------------------------
# Declarations
# ---------------------------------------------------------------------------

@dataclass
class Param(ASTNode):
    """A single formal parameter: name : type."""
    name: str = ""
    type: TypeNode = field(default_factory=TypeNode)


@dataclass
class FuncDecl(ASTNode):
    """Function declaration: func name(params) -> ret_type { body }."""
    name: str = ""
    params: List[Param] = field(default_factory=list)
    return_type: TypeNode = field(default_factory=TypeNode)
    body: "Block" = field(default_factory=lambda: Block())


# ---------------------------------------------------------------------------
# Statements
# ---------------------------------------------------------------------------

@dataclass
class Block(ASTNode):
    """A braces-enclosed list of statements."""
    statements: List[Any] = field(default_factory=list)


@dataclass
class VarDecl(ASTNode):
    """Variable declaration: let name [: type] = expr."""
    name: str = ""
    type_annotation: Optional[TypeNode] = None
    value: Any = None


@dataclass
class Assignment(ASTNode):
    """Variable assignment: name = expr."""
    name: str = ""
    value: Any = None


@dataclass
class IfStmt(ASTNode):
    """Conditional: if cond { then } [else { else_ }]."""
    condition: Any = None
    then_block: Block = field(default_factory=Block)
    else_block: Optional[Block] = None


@dataclass
class WhileStmt(ASTNode):
    """While loop: while cond { body }."""
    condition: Any = None
    body: Block = field(default_factory=Block)


@dataclass
class ForStmt(ASTNode):
    """Range-based for loop: for var in range(start, end) { body }."""
    var: str = ""
    start: Any = None
    end: Any = None
    body: Block = field(default_factory=Block)


@dataclass
class ReturnStmt(ASTNode):
    """Return statement: return [expr]."""
    value: Optional[Any] = None


@dataclass
class PrintStmt(ASTNode):
    """Built-in print call: print(expr)."""
    value: Any = None


@dataclass
class ExprStmt(ASTNode):
    """An expression used as a statement (e.g. a void function call)."""
    expr: Any = None


# ---------------------------------------------------------------------------
# Expressions
# ---------------------------------------------------------------------------

@dataclass
class BinOp(ASTNode):
    """Binary operation: left op right."""
    op: str = ""
    left: Any = None
    right: Any = None
    resolved_type: Optional[str] = field(default=None, compare=False, repr=False)


@dataclass
class UnaryOp(ASTNode):
    """Unary operation: op operand."""
    op: str = ""
    operand: Any = None
    resolved_type: Optional[str] = field(default=None, compare=False, repr=False)


@dataclass
class IntLiteral(ASTNode):
    """Integer literal."""
    value: int = 0
    resolved_type: str = field(default="int", compare=False, repr=False)


@dataclass
class FloatLiteral(ASTNode):
    """Floating-point literal."""
    value: float = 0.0
    resolved_type: str = field(default="float", compare=False, repr=False)


@dataclass
class StringLiteral(ASTNode):
    """String literal."""
    value: str = ""
    resolved_type: str = field(default="string", compare=False, repr=False)


@dataclass
class BoolLiteral(ASTNode):
    """Boolean literal (true / false)."""
    value: bool = False
    resolved_type: str = field(default="bool", compare=False, repr=False)


@dataclass
class Identifier(ASTNode):
    """Reference to a variable or parameter."""
    name: str = ""
    resolved_type: Optional[str] = field(default=None, compare=False, repr=False)


@dataclass
class FuncCall(ASTNode):
    """Function-call expression: name(args)."""
    name: str = ""
    args: List[Any] = field(default_factory=list)
    resolved_type: Optional[str] = field(default=None, compare=False, repr=False)
