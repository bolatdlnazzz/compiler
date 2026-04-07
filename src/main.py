"""
ZLang Compiler – Command-Line Driver.

Usage
─────
  python -m src.main <source.zlang>              # compile + run (default)
  python -m src.main <source.zlang> --emit-cpp   # print generated C++
  python -m src.main <source.zlang> --emit-ast   # print AST (pretty)
  python -m src.main <source.zlang> -o <output>  # write C++ to file

Pipeline
────────
  source text  →  Lexer  →  tokens
  tokens       →  Parser →  AST
  AST          →  SemanticAnalyzer  →  annotated AST
  annotated AST→  CodeGenerator    →  C++ source
"""
import argparse
import sys
import subprocess
import tempfile
import os

from .lexer import Lexer, LexerError
from .parser import Parser, ParseError
from .semantic import SemanticAnalyzer, SemanticError
from .codegen import CodeGenerator, CodeGenError


def compile_source(source: str) -> str:
    """
    Run the full ZLang compilation pipeline.

    Parameters
    ----------
    source : str
        ZLang source code.

    Returns
    -------
    str
        Generated C++ source code.

    Raises
    ------
    LexerError, ParseError, SemanticError, CodeGenError
        On the first error encountered in each phase.
    """
    # Phase 1: Lexical analysis
    lexer = Lexer(source)
    tokens = lexer.tokenize()

    # Phase 2: Syntactic analysis (parsing)
    parser = Parser(tokens)
    ast = parser.parse()

    # Phase 3: Semantic analysis (type checking + type inference)
    analyzer = SemanticAnalyzer()
    analyzer.analyze(ast)

    # Phase 4: C++ code generation
    generator = CodeGenerator()
    cpp_source = generator.generate(ast)

    return cpp_source


def main(argv=None) -> int:
    arg_parser = argparse.ArgumentParser(
        prog="zlang",
        description="ZLang compiler – transpiles ZLang source to C++",
    )
    arg_parser.add_argument("source", help="ZLang source file (.zlang)")
    arg_parser.add_argument(
        "--emit-cpp",
        action="store_true",
        help="Print generated C++ to stdout instead of compiling",
    )
    arg_parser.add_argument(
        "--emit-ast",
        action="store_true",
        help="Print the AST to stdout and exit",
    )
    arg_parser.add_argument(
        "-o", "--output",
        metavar="FILE",
        help="Write generated C++ to FILE (implies --emit-cpp without execution)",
    )
    args = arg_parser.parse_args(argv)

    # Read source
    try:
        with open(args.source, "r", encoding="utf-8") as fh:
            source = fh.read()
    except OSError as exc:
        print(f"Error: cannot read '{args.source}': {exc}", file=sys.stderr)
        return 1

    # Lexical + parse + semantic + codegen
    try:
        # AST-only mode
        if args.emit_ast:
            lexer = Lexer(source)
            tokens = lexer.tokenize()
            parser = Parser(tokens)
            ast = parser.parse()
            # Pretty-print the AST
            import pprint
            pprint.pprint(ast)
            return 0

        cpp_source = compile_source(source)

    except (LexerError, ParseError, SemanticError, CodeGenError) as exc:
        print(f"Compilation failed:\n  {exc}", file=sys.stderr)
        return 1

    # Write to file or stdout
    if args.output:
        try:
            with open(args.output, "w", encoding="utf-8") as fh:
                fh.write(cpp_source)
            print(f"C++ written to '{args.output}'")
        except OSError as exc:
            print(f"Error writing output: {exc}", file=sys.stderr)
            return 1
        return 0

    if args.emit_cpp:
        print(cpp_source, end="")
        return 0

    # Default: compile with g++ and run
    with tempfile.TemporaryDirectory() as tmpdir:
        cpp_path = os.path.join(tmpdir, "program.cpp")
        exe_path = os.path.join(tmpdir, "program")
        with open(cpp_path, "w", encoding="utf-8") as fh:
            fh.write(cpp_source)
        # Compile
        result = subprocess.run(
            ["g++", "-std=c++17", "-o", exe_path, cpp_path],
            capture_output=True, text=True,
        )
        if result.returncode != 0:
            print("g++ compilation failed:", file=sys.stderr)
            print(result.stderr, file=sys.stderr)
            return 1
        # Run
        result = subprocess.run([exe_path], text=True)
        return result.returncode


if __name__ == "__main__":
    sys.exit(main())
