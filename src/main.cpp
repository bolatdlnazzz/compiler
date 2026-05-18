#include "ast_dump.h"
#include "codegen.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifndef ASTRA_RUNTIME_C_PATH
#define ASTRA_RUNTIME_C_PATH "runtime.c"
#endif

namespace {

struct CliOptions {
    std::string sourceFile;
    std::string outputFile;
    bool dumpTokens = false;
    bool dumpAst = false;
    bool emitAsmOnly = false;
};

void printUsage(const char* argv0) {
    std::cerr
        << "Usage: " << argv0
        << " <source_file> [-o <output_file>] [--dump-tokens] [--dump-ast] [--emit-asm]\n";
}

std::string defaultOutputName(const std::string& sourceFile) {
    std::filesystem::path path(sourceFile);
    std::string stem = path.stem().string();

    if (stem.empty()) {
        return "a.out";
    }

    return stem;
}

bool parseArgs(int argc, char* argv[], CliOptions& options) {
    if (argc < 2) {
        return false;
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--dump-tokens") {
            options.dumpTokens = true;
        } else if (arg == "--dump-ast") {
            options.dumpAst = true;
        } else if (arg == "--emit-asm") {
            options.emitAsmOnly = true;
        } else if (arg == "-o") {
            if (i + 1 >= argc) {
                return false;
            }

            options.outputFile = argv[++i];
        } else if (!arg.empty() && arg[0] == '-') {
            return false;
        } else if (options.sourceFile.empty()) {
            options.sourceFile = arg;
        } else {
            return false;
        }
    }

    if (options.sourceFile.empty()) {
        return false;
    }

    if (options.outputFile.empty()) {
        options.outputFile = defaultOutputName(options.sourceFile);
    }

    return true;
}

bool readFile(const std::string& path, std::string& outSource) {
    std::ifstream file(path);

    if (!file.is_open()) {
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    outSource = buffer.str();

    return true;
}

void dumpTokens(const std::vector<Lexer::Token>& tokens) {
    std::cout << "=== TOKENS ===\n";

    for (const auto& token : tokens) {
        std::cout << token.pos.line << ":" << token.pos.column << "  "
                  << Lexer::tokenTypeToString(token.type) << "  "
                  << token.lexeme << "\n";
    }
}

std::string quoteShellArg(const std::string& value) {
    std::string result = "\"";

    for (char c : value) {
        if (c == '"' || c == '\\') {
            result += '\\';
        }

        result += c;
    }

    result += "\"";
    return result;
}

bool runCommand(const std::string& command) {
    int code = std::system(command.c_str());
    return code == 0;
}

} // namespace

int main(int argc, char* argv[]) {
    CliOptions options;

    if (!parseArgs(argc, argv, options)) {
        printUsage(argv[0]);
        return 1;
    }

    std::string source;

    if (!readFile(options.sourceFile, source)) {
        std::cerr << options.sourceFile
                  << ":1:1: error: cannot open source file\n";
        return 1;
    }

    // =========================
    // Lexer
    // =========================

    Lexer::Lexer lexer(source, options.sourceFile);
    auto tokens = lexer.tokenize();

    for (const auto& token : tokens) {
        if (token.type == Lexer::TokenType::Error) {
            std::cerr << lexer.formatError(token) << "\n";
            return 1;
        }
    }

    if (options.dumpTokens) {
        dumpTokens(tokens);
        return 0;
    }

    // =========================
    // Parser
    // =========================

    Parser::Parser parser(tokens, options.sourceFile);
    auto ast = parser.parseModule();

    if (!parser.diagnostics().empty()) {
        for (const auto& diagnostic : parser.diagnostics()) {
            std::cerr << Parser::formatDiagnostic(diagnostic) << "\n";
        }

        return 1;
    }

    if (options.dumpAst) {
        ASTDump::dumpModule(*ast, std::cout);
        return 0;
    }

    // =========================
    // Semantic analyzer
    // =========================

    Semantic::Analyzer semantic(options.sourceFile);
    bool semanticOk = semantic.analyze(*ast);

    if (!semanticOk) {
        for (const auto& diagnostic : semantic.diagnostics()) {
            std::cerr << Semantic::formatDiagnostic(diagnostic) << "\n";
        }

        return 1;
    }

    // =========================
    // Codegen
    // =========================

    std::string asmPath;

    if (options.emitAsmOnly) {
        asmPath = options.outputFile;
    } else {
        asmPath = options.outputFile + ".asm";
    }

    Codegen::Generator codegen(options.sourceFile);

    if (!codegen.generate(*ast, asmPath)) {
        for (const auto& diagnostic : codegen.diagnostics()) {
            std::cerr << Codegen::formatDiagnostic(diagnostic) << "\n";
        }

        return 1;
    }

    if (options.emitAsmOnly) {
        std::cout << "Generated asm: " << asmPath << "\n";
        return 0;
    }

    const std::string objectPath = options.outputFile + ".o";

    const std::string nasmCommand =
        "nasm -felf64 " + quoteShellArg(asmPath) +
        " -o " + quoteShellArg(objectPath);

    if (!runCommand(nasmCommand)) {
        std::cerr << options.sourceFile
                  << ":1:1: error: nasm failed. Install nasm or use --emit-asm\n";
        return 1;
    }

    const std::string linkCommand =
        "cc -no-pie " +
        quoteShellArg(objectPath) + " " +
        quoteShellArg(ASTRA_RUNTIME_C_PATH) +
        " -o " + quoteShellArg(options.outputFile);

    if (!runCommand(linkCommand)) {
        std::cerr << options.sourceFile
                  << ":1:1: error: linker failed while creating executable\n";
        return 1;
    }

    std::cout << "Compilation successful: " << options.outputFile << "\n";
    return 0;
}