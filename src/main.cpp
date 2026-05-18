#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "lexer.h"
#include "parser.h"
#include "semantic.h"

static std::string joinPath(const std::vector<std::string>& path) {
    std::string result;
    for (const auto& part : path) {
        if (!result.empty()) result += "::";
        result += part;
    }
    return result;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <source_file>" << std::endl;
        return 1;
    }

    // Читаем исходный файл
    std::ifstream file(argv[1]);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file " << argv[1] << std::endl;
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    file.close();

    // Лексический анализ
    Lexer::Lexer lexer(source, argv[1]);
    auto tokens = lexer.tokenize();

    for (const auto& token : tokens) {
        if (token.type == Lexer::TokenType::Error) {
            std::cerr << lexer.formatError(token) << std::endl;
            return 1;
        }
    }
    
    std::cout << "=== LEXER ===" << std::endl;
    std::cout << "Tokens: " << tokens.size() << std::endl;
    for (const auto& token : tokens) {
        if (token.type != Lexer::TokenType::EndOfFile) {
            std::cout << "  [" << static_cast<int>(token.type) << "] " 
                      << token.lexeme << std::endl;
        }
    }

    // Синтаксический анализ
    std::cout << "\n=== PARSER ===" << std::endl;
    Parser::Parser parser(tokens, argv[1]);
    auto ast = parser.parseModule();
    if (!ast->namePath.empty()) {
        std::cout << "Module: " << joinPath(ast->namePath) << std::endl;
    }
    
    if (!parser.diagnostics().empty()) {
        std::cerr << "Parse errors: " << parser.diagnostics().size() << std::endl;
        for (const auto& diag : parser.diagnostics()) {
            std::cerr << "  " << diag.message << std::endl;
        }
        
        std::cout << "\nCompilation failed!" << std::endl;
        return 1;
    }

    std::cout << "Parse OK" << std::endl;

    // Семантический анализ
    std::cout << "\n=== SEMANTIC ===" << std::endl;
    Semantic::Analyzer semantic(argv[1]);
    bool sem_ok = semantic.analyze(*ast);
    
    if (!semantic.diagnostics().empty()) {
        std::cerr << "Semantic errors: " << semantic.diagnostics().size() << std::endl;
        for (const auto& diag : semantic.diagnostics()) {
            std::cerr << "  " << diag.message << std::endl;
        }
    } else {
        std::cout << "Semantic OK" << std::endl;
    }

    if (sem_ok && parser.diagnostics().empty()) {
        std::cout << "\nCompilation successful!" << std::endl;
    } else {
        std::cout << "\nCompilation failed!" << std::endl;
        return 1;
    }
    return 0;
}
