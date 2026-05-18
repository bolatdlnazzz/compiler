
## My compiler

This patch adds:

- explicit optional module header: `module Name;`
- CLI required by ТЗ: `<source file> [-o <output>] --dump-tokens --dump-ast`
- independent `Codegen::` phase after semantic analysis
- NASM x86-64 assembly generation for scalar int/bool/string-literal programs
- runtime support file `runtime.c`
- AST dump support
- unified diagnostics format `<file>:<line>:<column>: error: ...`

Current backend limitations are reported as codegen errors, not ignored silently:

- arrays and structs are semantically checked, but native layout/codegen is not complete yet;
- float codegen is not complete yet;
- string operations are limited to string literals, `print`, `input`, `len`, `panic`.

Use:

```bash
cmake --build .
./src/astra examples/basics/good.astra --dump-tokens
./src/astra examples/basics/good.astra --dump-ast
./src/astra examples/basics/good.astra --emit-asm -o out.asm
./src/astra examples/basics/good.astra -o out
```