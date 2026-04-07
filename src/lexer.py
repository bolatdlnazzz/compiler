"""
Lexical Analyzer (Lexer) for ZLang.

Converts source-code text into a flat sequence of tokens that the parser
can consume.  Single-line comments start with '#'.  Whitespace (including
newlines) is silently skipped between tokens.
"""
from enum import Enum, auto
from dataclasses import dataclass
from typing import List, Optional


# ---------------------------------------------------------------------------
# Token types
# ---------------------------------------------------------------------------

class TokenType(Enum):
    # Literals
    INTEGER = auto()
    FLOAT   = auto()
    STRING  = auto()
    BOOL    = auto()

    # Identifier (variable / function name that is not a reserved word)
    IDENT = auto()

    # Reserved keywords
    FUNC   = auto()
    LET    = auto()
    IF     = auto()
    ELSE   = auto()
    WHILE  = auto()
    FOR    = auto()
    IN     = auto()
    RANGE  = auto()
    RETURN = auto()
    PRINT  = auto()
    AND    = auto()
    OR     = auto()
    NOT    = auto()

    # Built-in type keywords
    TYPE_INT    = auto()
    TYPE_FLOAT  = auto()
    TYPE_BOOL   = auto()
    TYPE_STRING = auto()
    TYPE_VOID   = auto()

    # Arithmetic operators
    PLUS    = auto()   # +
    MINUS   = auto()   # -
    STAR    = auto()   # *
    SLASH   = auto()   # /
    PERCENT = auto()   # %

    # Relational / assignment operators
    ASSIGN = auto()    # =
    EQ     = auto()    # ==
    NEQ    = auto()    # !=
    LT     = auto()    # <
    GT     = auto()    # >
    LTE    = auto()    # <=
    GTE    = auto()    # >=

    # Punctuation / delimiters
    LPAREN = auto()    # (
    RPAREN = auto()    # )
    LBRACE = auto()    # {
    RBRACE = auto()    # }
    COLON  = auto()    # :
    COMMA  = auto()    # ,
    ARROW  = auto()    # ->

    # Sentinel
    EOF = auto()


# Mapping from literal keyword text to TokenType
KEYWORDS: dict = {
    "func":   TokenType.FUNC,
    "let":    TokenType.LET,
    "if":     TokenType.IF,
    "else":   TokenType.ELSE,
    "while":  TokenType.WHILE,
    "for":    TokenType.FOR,
    "in":     TokenType.IN,
    "range":  TokenType.RANGE,
    "return": TokenType.RETURN,
    "print":  TokenType.PRINT,
    "true":   TokenType.BOOL,
    "false":  TokenType.BOOL,
    "and":    TokenType.AND,
    "or":     TokenType.OR,
    "not":    TokenType.NOT,
    "int":    TokenType.TYPE_INT,
    "float":  TokenType.TYPE_FLOAT,
    "bool":   TokenType.TYPE_BOOL,
    "string": TokenType.TYPE_STRING,
    "void":   TokenType.TYPE_VOID,
}


# ---------------------------------------------------------------------------
# Token
# ---------------------------------------------------------------------------

@dataclass
class Token:
    type: TokenType
    value: object
    line: int
    col: int

    def __repr__(self) -> str:
        return f"Token({self.type.name}, {self.value!r}, line={self.line}, col={self.col})"


# ---------------------------------------------------------------------------
# Error
# ---------------------------------------------------------------------------

class LexerError(Exception):
    def __init__(self, message: str, line: int, col: int) -> None:
        super().__init__(f"Lexer error at line {line}, col {col}: {message}")
        self.line = line
        self.col = col


# ---------------------------------------------------------------------------
# Lexer
# ---------------------------------------------------------------------------

class Lexer:
    """Tokenises a ZLang source string into a list of Tokens."""

    def __init__(self, source: str) -> None:
        self.source = source
        self.pos = 0
        self.line = 1
        self.col = 1

    # ── Internal helpers ─────────────────────────────────────────────────

    def _error(self, msg: str) -> None:
        raise LexerError(msg, self.line, self.col)

    def _peek(self, offset: int = 0) -> Optional[str]:
        p = self.pos + offset
        return self.source[p] if p < len(self.source) else None

    def _advance(self) -> str:
        ch = self.source[self.pos]
        self.pos += 1
        if ch == "\n":
            self.line += 1
            self.col = 1
        else:
            self.col += 1
        return ch

    def _skip_whitespace_and_comments(self) -> None:
        while self.pos < len(self.source):
            ch = self._peek()
            if ch in (" ", "\t", "\r", "\n"):
                self._advance()
            elif ch == "#":
                # single-line comment: skip until end-of-line
                while self.pos < len(self.source) and self._peek() != "\n":
                    self._advance()
            else:
                break

    # ── Token-type readers ────────────────────────────────────────────────

    def _read_string(self, line: int, col: int) -> Token:
        self._advance()  # consume opening '"'
        chars: List[str] = []
        while self.pos < len(self.source):
            ch = self._peek()
            if ch == '"':
                self._advance()
                return Token(TokenType.STRING, "".join(chars), line, col)
            if ch == "\\":
                self._advance()
                esc = self._advance()
                mapping = {"n": "\n", "t": "\t", "\\": "\\", '"': '"'}
                if esc in mapping:
                    chars.append(mapping[esc])
                else:
                    self._error(f"Unknown escape sequence: \\{esc}")
            else:
                chars.append(self._advance())
        self._error("Unterminated string literal")

    def _read_number(self, line: int, col: int) -> Token:
        start = self.pos
        is_float = False
        while self.pos < len(self.source) and self._peek().isdigit():
            self._advance()
        if (
            self.pos < len(self.source)
            and self._peek() == "."
            and self._peek(1) is not None
            and self._peek(1).isdigit()
        ):
            is_float = True
            self._advance()  # consume '.'
            while self.pos < len(self.source) and self._peek().isdigit():
                self._advance()
        text = self.source[start: self.pos]
        if is_float:
            return Token(TokenType.FLOAT, float(text), line, col)
        return Token(TokenType.INTEGER, int(text), line, col)

    def _read_ident_or_keyword(self, line: int, col: int) -> Token:
        start = self.pos
        while self.pos < len(self.source) and (
            self._peek().isalnum() or self._peek() == "_"
        ):
            self._advance()
        text = self.source[start: self.pos]
        ttype = KEYWORDS.get(text, TokenType.IDENT)
        # 'true' / 'false' are BOOL literals; store their Python bool value
        if ttype == TokenType.BOOL:
            return Token(TokenType.BOOL, text == "true", line, col)
        return Token(ttype, text, line, col)

    # ── Public interface ──────────────────────────────────────────────────

    def tokenize(self) -> List[Token]:
        """Return the complete token list ending with an EOF token."""
        tokens: List[Token] = []

        while True:
            self._skip_whitespace_and_comments()
            if self.pos >= len(self.source):
                tokens.append(Token(TokenType.EOF, None, self.line, self.col))
                break

            line, col = self.line, self.col
            ch = self._peek()

            if ch == '"':
                tokens.append(self._read_string(line, col))
            elif ch.isdigit():
                tokens.append(self._read_number(line, col))
            elif ch.isalpha() or ch == "_":
                tokens.append(self._read_ident_or_keyword(line, col))
            elif ch == "+":
                self._advance()
                tokens.append(Token(TokenType.PLUS, "+", line, col))
            elif ch == "-":
                self._advance()
                if self._peek() == ">":
                    self._advance()
                    tokens.append(Token(TokenType.ARROW, "->", line, col))
                else:
                    tokens.append(Token(TokenType.MINUS, "-", line, col))
            elif ch == "*":
                self._advance()
                tokens.append(Token(TokenType.STAR, "*", line, col))
            elif ch == "/":
                self._advance()
                tokens.append(Token(TokenType.SLASH, "/", line, col))
            elif ch == "%":
                self._advance()
                tokens.append(Token(TokenType.PERCENT, "%", line, col))
            elif ch == "=":
                self._advance()
                if self._peek() == "=":
                    self._advance()
                    tokens.append(Token(TokenType.EQ, "==", line, col))
                else:
                    tokens.append(Token(TokenType.ASSIGN, "=", line, col))
            elif ch == "!":
                self._advance()
                if self._peek() == "=":
                    self._advance()
                    tokens.append(Token(TokenType.NEQ, "!=", line, col))
                else:
                    tokens.append(Token(TokenType.NOT, "!", line, col))
            elif ch == "<":
                self._advance()
                if self._peek() == "=":
                    self._advance()
                    tokens.append(Token(TokenType.LTE, "<=", line, col))
                else:
                    tokens.append(Token(TokenType.LT, "<", line, col))
            elif ch == ">":
                self._advance()
                if self._peek() == "=":
                    self._advance()
                    tokens.append(Token(TokenType.GTE, ">=", line, col))
                else:
                    tokens.append(Token(TokenType.GT, ">", line, col))
            elif ch == "(":
                self._advance()
                tokens.append(Token(TokenType.LPAREN, "(", line, col))
            elif ch == ")":
                self._advance()
                tokens.append(Token(TokenType.RPAREN, ")", line, col))
            elif ch == "{":
                self._advance()
                tokens.append(Token(TokenType.LBRACE, "{", line, col))
            elif ch == "}":
                self._advance()
                tokens.append(Token(TokenType.RBRACE, "}", line, col))
            elif ch == ":":
                self._advance()
                tokens.append(Token(TokenType.COLON, ":", line, col))
            elif ch == ",":
                self._advance()
                tokens.append(Token(TokenType.COMMA, ",", line, col))
            else:
                self._error(f"Unexpected character: {ch!r}")

        return tokens
