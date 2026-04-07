"""
Unit tests for the ZLang Lexer.
"""
import pytest
from src.lexer import Lexer, LexerError, Token, TokenType


def tokenize(source: str):
    return Lexer(source).tokenize()


def ttypes(source: str):
    """Return only the token types (excluding EOF)."""
    return [t.type for t in tokenize(source) if t.type != TokenType.EOF]


def tvalues(source: str):
    """Return (type, value) pairs (excluding EOF)."""
    return [(t.type, t.value) for t in tokenize(source) if t.type != TokenType.EOF]


# ── Integer literals ──────────────────────────────────────────────────────

class TestIntegerLiterals:
    def test_zero(self):
        assert tvalues("0") == [(TokenType.INTEGER, 0)]

    def test_positive(self):
        assert tvalues("42") == [(TokenType.INTEGER, 42)]

    def test_multi_digit(self):
        assert tvalues("1234567890") == [(TokenType.INTEGER, 1234567890)]


# ── Float literals ────────────────────────────────────────────────────────

class TestFloatLiterals:
    def test_simple(self):
        assert tvalues("3.14") == [(TokenType.FLOAT, 3.14)]

    def test_leading_digit(self):
        assert tvalues("0.5") == [(TokenType.FLOAT, 0.5)]

    def test_no_plain_dot(self):
        # "1." is not a valid token: '.' without a following digit raises LexerError
        with pytest.raises(LexerError):
            tokenize("1.")


# ── String literals ───────────────────────────────────────────────────────

class TestStringLiterals:
    def test_empty(self):
        assert tvalues('""') == [(TokenType.STRING, "")]

    def test_simple(self):
        assert tvalues('"hello"') == [(TokenType.STRING, "hello")]

    def test_escape_newline(self):
        assert tvalues(r'"a\nb"') == [(TokenType.STRING, "a\nb")]

    def test_escape_tab(self):
        assert tvalues(r'"a\tb"') == [(TokenType.STRING, "a\tb")]

    def test_escape_quote(self):
        assert tvalues(r'"say \"hi\""') == [(TokenType.STRING, 'say "hi"')]

    def test_escape_backslash(self):
        assert tvalues(r'"a\\b"') == [(TokenType.STRING, "a\\b")]

    def test_unterminated(self):
        with pytest.raises(LexerError):
            tokenize('"unterminated')


# ── Boolean literals ──────────────────────────────────────────────────────

class TestBoolLiterals:
    def test_true(self):
        assert tvalues("true") == [(TokenType.BOOL, True)]

    def test_false(self):
        assert tvalues("false") == [(TokenType.BOOL, False)]


# ── Keywords ──────────────────────────────────────────────────────────────

class TestKeywords:
    @pytest.mark.parametrize("kw, expected", [
        ("func",   TokenType.FUNC),
        ("let",    TokenType.LET),
        ("if",     TokenType.IF),
        ("else",   TokenType.ELSE),
        ("while",  TokenType.WHILE),
        ("for",    TokenType.FOR),
        ("in",     TokenType.IN),
        ("range",  TokenType.RANGE),
        ("return", TokenType.RETURN),
        ("print",  TokenType.PRINT),
        ("and",    TokenType.AND),
        ("or",     TokenType.OR),
        ("not",    TokenType.NOT),
        ("int",    TokenType.TYPE_INT),
        ("float",  TokenType.TYPE_FLOAT),
        ("bool",   TokenType.TYPE_BOOL),
        ("string", TokenType.TYPE_STRING),
        ("void",   TokenType.TYPE_VOID),
    ])
    def test_keyword(self, kw, expected):
        assert ttypes(kw) == [expected]


# ── Identifiers ───────────────────────────────────────────────────────────

class TestIdentifiers:
    def test_simple(self):
        assert tvalues("x") == [(TokenType.IDENT, "x")]

    def test_with_digits(self):
        assert tvalues("x1") == [(TokenType.IDENT, "x1")]

    def test_underscore(self):
        assert tvalues("_my_var") == [(TokenType.IDENT, "_my_var")]

    def test_uppercase(self):
        assert tvalues("MyFunc") == [(TokenType.IDENT, "MyFunc")]


# ── Operators ─────────────────────────────────────────────────────────────

class TestOperators:
    @pytest.mark.parametrize("src, expected", [
        ("+",  TokenType.PLUS),
        ("-",  TokenType.MINUS),
        ("*",  TokenType.STAR),
        ("/",  TokenType.SLASH),
        ("%",  TokenType.PERCENT),
        ("=",  TokenType.ASSIGN),
        ("==", TokenType.EQ),
        ("!=", TokenType.NEQ),
        ("<",  TokenType.LT),
        (">",  TokenType.GT),
        ("<=", TokenType.LTE),
        (">=", TokenType.GTE),
        ("->", TokenType.ARROW),
    ])
    def test_operator(self, src, expected):
        assert ttypes(src) == [expected]


# ── Punctuation ───────────────────────────────────────────────────────────

class TestPunctuation:
    @pytest.mark.parametrize("src, expected", [
        ("(", TokenType.LPAREN),
        (")", TokenType.RPAREN),
        ("{", TokenType.LBRACE),
        ("}", TokenType.RBRACE),
        (":", TokenType.COLON),
        (",", TokenType.COMMA),
    ])
    def test_punct(self, src, expected):
        assert ttypes(src) == [expected]


# ── Comments ─────────────────────────────────────────────────────────────

class TestComments:
    def test_single_line_comment_is_ignored(self):
        assert ttypes("# this is a comment") == []

    def test_comment_after_token(self):
        assert ttypes("42 # the answer") == [TokenType.INTEGER]

    def test_multiline_with_comments(self):
        src = "x # first\ny # second"
        types = ttypes(src)
        assert types == [TokenType.IDENT, TokenType.IDENT]


# ── Whitespace ────────────────────────────────────────────────────────────

class TestWhitespace:
    def test_spaces_between_tokens(self):
        assert tvalues("1 + 2") == [
            (TokenType.INTEGER, 1),
            (TokenType.PLUS, "+"),
            (TokenType.INTEGER, 2),
        ]

    def test_newlines_ignored(self):
        assert ttypes("x\ny") == [TokenType.IDENT, TokenType.IDENT]


# ── Error cases ───────────────────────────────────────────────────────────

class TestErrors:
    def test_unexpected_char(self):
        with pytest.raises(LexerError):
            tokenize("@")

    def test_unexpected_tilde(self):
        with pytest.raises(LexerError):
            tokenize("~x")


# ── EOF sentinel ─────────────────────────────────────────────────────────

class TestEOF:
    def test_empty_source(self):
        toks = tokenize("")
        assert toks[-1].type == TokenType.EOF

    def test_eof_after_tokens(self):
        toks = tokenize("x")
        assert toks[-1].type == TokenType.EOF


# ── Location tracking ────────────────────────────────────────────────────

class TestLocations:
    def test_single_token_line_col(self):
        toks = tokenize("x")
        assert toks[0].line == 1
        assert toks[0].col == 1

    def test_second_line(self):
        toks = tokenize("x\ny")
        assert toks[1].line == 2

    def test_column_tracking(self):
        toks = tokenize("  hello")
        assert toks[0].col == 3
