// Этот файл - главная точка входа (entry point) компилятора.
// Здесь происходит:
// 1. Парсинг аргументов командной строки
// 2. Чтение исходного файла
// 3. Вызов всех этапов компиляции в нужном порядке:
//    - Лексический анализ (Lexer) - разбиение на токены
//    - Синтаксический анализ (Parser) - построение AST
//    - Семантический анализ (Semantic) - проверка типов и правильности
//    - Генерация кода (Codegen) - создание ассемблера
//    - Компиляция ассемблера в object-файл (nasm)
//    - Линковка с runtime библиотекой

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

// Структура для хранения опций из командной строки
// dumpTokens: если true, выводим список токенов и выходим (для отладки)
// dumpAst: если true, выводим дерево синтаксиса и выходим (для отладки)
// emitAsmOnly: если true, генерируем только .asm файл, не компилируем дальше
struct CliOptions {
    std::string sourceFile;      // путь к исходному файлу на Astra
    std::string outputFile;      // путь к итоговому исполняемому файлу
    bool dumpTokens = false;     // опция --dump-tokens
    bool dumpAst = false;        // опция --dump-ast
    bool emitAsmOnly = false;    // опция --emit-asm
};

// Показываем пользователю, как правильно использовать компилятор
void printUsage(const char* argv0) {
    std::cerr
        << "Usage: " << argv0
        << " <source_file> [-o <output_file>] [--dump-tokens] [--dump-ast] [--emit-asm]\n";
}

// Если пользователь не указал имя выходного файла (-o),
// используем имя исходного файла (без расширения)
// Например: "hello.astra" -> "hello"
std::string defaultOutputName(const std::string& sourceFile) {
    std::filesystem::path path(sourceFile);
    std::string stem = path.stem().string();

    if (stem.empty()) {
        return "a.out";  // стандартное имя по умолчанию
    }

    return stem;
}

// Парсим аргументы командной строки и заполняем структуру CliOptions
bool parseArgs(int argc, char* argv[], CliOptions& options) {
    if (argc < 2) {
        return false;  // должен быть хотя бы один аргумент (имя файла)
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        // Проверяем флаги
        if (arg == "--dump-tokens") {
            options.dumpTokens = true;  // режим отладки: вывести токены
        } else if (arg == "--dump-ast") {
            options.dumpAst = true;     // режим отладки: вывести AST
        } else if (arg == "--emit-asm") {
            options.emitAsmOnly = true; // режим отладки: только генерировать ассемблер
        } else if (arg == "-o") {
            // -o это флаг для указания имени выходного файла
            if (i + 1 >= argc) {
                return false;  // после -o должно быть значение
            }
            options.outputFile = argv[++i];
        } else if (!arg.empty() && arg[0] == '-') {
            return false;  // неизвестный флаг
        } else if (options.sourceFile.empty()) {
            options.sourceFile = arg;  // первый позиционный аргумент - исходный файл
        } else {
            return false;  // слишком много позиционных аргументов
        }
    }

    if (options.sourceFile.empty()) {
        return false;  // исходный файл обязателен
    }

    if (options.outputFile.empty()) {
        options.outputFile = defaultOutputName(options.sourceFile);
    }

    return true;
}

// Читаем исходный файл целиком в одну строку
bool readFile(const std::string& path, std::string& outSource) {
    std::ifstream file(path);

    if (!file.is_open()) {
        return false;  // файл не может быть открыт
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();  // читаем весь файл
    outSource = buffer.str();

    return true;
}

// Выводим все токены (используется при --dump-tokens)
// Полезно для отладки лексера
void dumpTokens(const std::vector<Lexer::Token>& tokens) {
    std::cout << "=== TOKENS ===\n";

    for (const auto& token : tokens) {
        // Для каждого токена выводим: строка:колонна, тип токена, текст
        std::cout << token.pos.line << ":" << token.pos.column << "  "
                  << Lexer::tokenTypeToString(token.type) << "  "
                  << token.lexeme << "\n";
    }
}

// Экранируем строку для безопасного использования в shell команде
// Обрабатываем кавычки и backslash-и
std::string quoteShellArg(const std::string& value) {
    std::string result = "\"";

    for (char c : value) {
        // Экранируем специальные символы
        if (c == '"' || c == '\\') {
            result += '\\';
        }
        result += c;
    }

    result += "\"";
    return result;
}

// Запускаем shell команду и возвращаем true если она успешно выполнена (exit code = 0)
bool runCommand(const std::string& command) {
    int code = std::system(command.c_str());
    return code == 0;
}

} // namespace

int main(int argc, char* argv[]) {
    // Парсим команду
    CliOptions options;

    if (!parseArgs(argc, argv, options)) {
        printUsage(argv[0]);
        return 1;
    }

    // Читаем исходный файл
    std::string source;

    if (!readFile(options.sourceFile, source)) {
        std::cerr << options.sourceFile
                  << ":1:1: error: cannot open source file\n";
        return 1;
    }

    // ========== ЭТАП 1: ЛЕКСИЧЕСКИЙ АНАЛИЗ (LEXER) ==========
    // Лексер разбивает текст исходного кода на токены (слова, операторы, скобки и т.д.)
    // Например: "let x = 5;" -> [KEYWORD(let), IDENTIFIER(x), OP(=), INT(5), SEP(;)]

    Lexer::Lexer lexer(source, options.sourceFile);
    auto tokens = lexer.tokenize();  // преобразуем весь код в вектор токенов

    // Проверяем, не было ли ошибок при токенизации
    for (const auto& token : tokens) {
        if (token.type == Lexer::TokenType::Error) {
            std::cerr << lexer.formatError(token) << "\n";
            return 1;
        }
    }

    // Если флаг --dump-tokens, показываем токены и выходим (полезно для отладки)
    if (options.dumpTokens) {
        dumpTokens(tokens);
        return 0;
    }

    // ========== ЭТАП 2: СИНТАКСИЧЕСКИЙ АНАЛИЗ (PARSER) ==========
    // Парсер берет список токенов и строит Abstract Syntax Tree (AST)
    // AST - это дерево, которое отображает структуру программы
    // Парсер проверяет грамматику: правильный порядок слов, скобки и т.д.

    Parser::Parser parser(tokens, options.sourceFile);
    auto ast = parser.parseModule();  // строим AST из токенов

    // Если были синтаксические ошибки, выводим их и выходим
    if (!parser.diagnostics().empty()) {
        for (const auto& diagnostic : parser.diagnostics()) {
            std::cerr << Parser::formatDiagnostic(diagnostic) << "\n";
        }
        return 1;
    }

    // Если флаг --dump-ast, показываем дерево синтаксиса и выходим
    if (options.dumpAst) {
        ASTDump::dumpModule(*ast, std::cout);
        return 0;
    }

    // ========== ЭТАП 3: СЕМАНТИЧЕСКИЙ АНАЛИЗ (SEMANTIC) ==========
    // Семантический анализ проверяет смысл программы:
    // - Все ли переменные объявлены перед использованием?
    // - Правильные ли типы данных?
    // - Есть ли функции в области видимости?
    // - Правильное ли число аргументов у функций?
    // Если ошибок нет, то дерево синтаксиса помечается типами

    Semantic::Analyzer semantic(options.sourceFile);
    bool semanticOk = semantic.analyze(*ast);  // анализируем AST

    // Если были ошибки типизации или семантики, выводим их
    if (!semanticOk) {
        for (const auto& diagnostic : semantic.diagnostics()) {
            std::cerr << Semantic::formatDiagnostic(diagnostic) << "\n";
        }
        return 1;
    }

    // ========== ЭТАП 4: ГЕНЕРАЦИЯ КОДА (CODEGEN) ==========
    // Кодегенератор преобразует проверенный AST в ассемблер x86-64
    // Это самый сложный этап: нужно выделять переменные на стеке,
    // генерировать инструкции процессора, вызывать встроенные функции

    std::string asmPath;  // путь к ассемблер файлу

    if (options.emitAsmOnly) {
        asmPath = options.outputFile;  // если --emit-asm, то не добавляем расширение
    } else {
        asmPath = options.outputFile + ".asm";  // обычно делаем .asm файл
    }

    Codegen::Generator codegen(options.sourceFile);

    // Генерируем ассемблер из AST
    if (!codegen.generate(*ast, asmPath)) {
        // Если были ошибки (например, неподдерживаемые операции)
        for (const auto& diagnostic : codegen.diagnostics()) {
            std::cerr << Codegen::formatDiagnostic(diagnostic) << "\n";
        }
        return 1;
    }

    // Если пользователь просил только ассемблер (--emit-asm), выходим
    if (options.emitAsmOnly) {
        std::cout << "Generated asm: " << asmPath << "\n";
        return 0;
    }

    // ========== ЭТАП 5: КОМПИЛЯЦИЯ АССЕМБЛЕРА В OBJECT FILE ==========
    // Используем nasm (Netwide Assembler) для преобразования ассемблера в machine code

    const std::string objectPath = options.outputFile + ".o";

    const std::string nasmCommand =
        "nasm -felf64 " + quoteShellArg(asmPath) +  // -felf64 это формат для x86-64 Linux
        " -o " + quoteShellArg(objectPath);

    if (!runCommand(nasmCommand)) {
        std::cerr << options.sourceFile
                  << ":1:1: error: nasm failed. Install nasm or use --emit-asm\n";
        return 1;
    }

    // ========== ЭТАП 6: ЛИНКОВКА ==========
    // Линкер (cc = C compiler) объединяет:
    // 1. Object файл с нашим кодом
    // 2. runtime.c с вспомогательными функциями (print, input и т.д.)
    // В результате получаем исполняемый файл

    const std::string linkCommand =
        "cc -no-pie " +  // -no-pie отключает position-independent executable
        quoteShellArg(objectPath) + " " +
        quoteShellArg(ASTRA_RUNTIME_C_PATH) +  // путь к runtime.c
        " -o " + quoteShellArg(options.outputFile);

    if (!runCommand(linkCommand)) {
        std::cerr << options.sourceFile
                  << ":1:1: error: linker failed while creating executable\n";
        return 1;
    }

    std::cout << "Compilation successful: " << options.outputFile << "\n";
    return 0;
}