#pragma once
// Семантический анализатор - третий этап компилятора.
// Его задача: проверить смысл и правильность программы
// Проверки:
//   1. Все ли переменные объявлены не только использованы?
//   2. Правильные ли типы всех выражений? (не добавляем строку к числу)
//   3. Все ли функции и типы достапны в области видимости? (не используем переменную до её объявления)
// О успешном анализе каждый узел AST помечается каноническим типом

#include "ast.h"
#include "lexer.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Semantic {

// Информация об ошибке семантики
struct Diagnostic {
    std::string file;         // какой файл?
    Lexer::Position pos{};    // где в файле?
    std::string message;      // что случилось?
};

// Отстраивают тип данных
// Каждый тип имеет энум kind, который говорит что фактически это:
struct Type {
    enum class Kind {
        Error,    // чтото сломалось
        Unit,     // () - "пустое" значение (возвращаемое функцией без return)
        Bool,     // true/false
        String,   // "hello"
        Int,      // сигнированные целые: int32, int64
        UInt,     // бессигнированные целые: uint32
        Float,    // вещественные: float32, float64
        Array,    // [int32; 10] - массив
        Struct    // struct Point { x: int32, y: int32 }
    };

    Kind kind = Kind::Error;
    int bits = 0;              // размер типа в битах (32 для int32)
    std::string name;          // имя типа (int32, uint64, Point)
    std::shared_ptr<Type> elementType;  // тип элементов для массивов
    std::uint64_t arraySize = 0;        // размер массива

    // Типовые константы для создания часто используемых типов
    static Type error();
    static Type unit();
    static Type boolean();
    static Type string();
    static Type integer(std::string name, int bits, bool isUnsigned);
    static Type floating(std::string name, int bits);
    static Type array(Type element, std::uint64_t size);
    static Type structure(std::string qualifiedName);

    bool operator==(const Type& other) const;
    bool operator!=(const Type& other) const { return !(*this == other); }

    bool isError() const { return kind == Kind::Error; }
    bool isNumeric() const { return kind == Kind::Int || kind == Kind::UInt || kind == Kind::Float; }
    bool isInteger() const { return kind == Kind::Int || kind == Kind::UInt; }
    bool isFloat() const { return kind == Kind::Float; }
    std::string toString() const;  // преобразовать в строку для показа
};

// Главный класс семантического анализатора
class Analyzer {
public:
    // Конструктор - инициализируем анализатор
    explicit Analyzer(std::string fileName = "<input>");

    // Главный метод - анализируем AST
    bool analyze(AST::Module& module);
    
    // Получить все найденные ошибки
    const std::vector<Diagnostic>& diagnostics() const noexcept { return diagnostics_; }

private:
    // Здесь проходит вся внутренняя логика
    // Контекст: таблицы символов, основные данные бытия

    struct Scope;          // область видимости
    struct FunctionInfo;   // инфо о функции
    struct StructInfo;     // инфо о structе

    enum class SymbolKind { Variable, Function, Struct, Alias, Namespace };

    struct Symbol {        // символ (переменная, функция, тип)
        SymbolKind kind = SymbolKind::Variable;
        std::string name;
        Type type = Type::error();
        bool isMutable = false;     // переменная?
        std::shared_ptr<FunctionInfo> function;
        std::shared_ptr<StructInfo> structure;
        Scope* namespaceScope = nullptr;  // если это namespace
    };

    struct Scope {         // таблица символов в области видимости
        Scope* parent = nullptr;  // родительская область (для нестед scope)
        bool isNamespace = false;
        std::string qualifiedName;  // если namespace: Geometry::Point
        std::unordered_map<std::string, Symbol> symbols;  // таблица символов
    };

    struct FunctionInfo {
        std::string name;
        std::string qualifiedName;
        std::vector<Type> paramTypes;
        Type returnType = Type::unit();
        AST::FunctionDecl* decl = nullptr;
        bool isBuiltin = false;
        std::string builtinName;
    };

    struct StructInfo {
        std::string name;
        std::string qualifiedName;
        std::vector<std::pair<std::string, Type>> fieldsInOrder;
        std::unordered_map<std::string, Type> fields;
    };

    struct LValueInfo {
        Type type = Type::error();
        bool isLValue = false;
        bool isMutable = false;
    };

    enum class Flow { MayContinue, NoContinue };

    std::string fileName_;
    std::vector<Diagnostic> diagnostics_;
    std::vector<std::unique_ptr<Scope>> ownedScopes_;  // все таблицы символов
    Scope* rootScope_ = nullptr;     // корневая scope
    Scope* moduleScope_ = nullptr;   // текущая модуль scope
    Scope* currentScope_ = nullptr;  // текущая scope
    std::vector<std::string> namespaceStack_;
    
    // Контекст для анализа (актуальные данные во время анализа)
    Type currentReturnType_ = Type::unit();  // экспектируемый выход функции
    int loopDepth_ = 0;                     // глубина вложенных циклов

    // Вспомогательные методы для работы со символами и областями
    Scope* makeScope(Scope* parent, bool isNamespace, std::string qualifiedName = {});
    Scope* ensureNamespace(Scope& scope, const std::string& name, const AST::Node& node);
    std::string qualify(std::string_view name) const;
    std::string joinPath(const std::vector<std::string>& path) const;
    void addDiagnostic(const AST::Node& node, std::string message);
    void addDiagnostic(Lexer::Position pos, std::string message);
    bool hasErrors() const { return !diagnostics_.empty(); }
    void installBuiltins();  // добавляем встроенные функции: print, input, panic
    void addBuiltinFunction(std::string name, Type returnType, std::vector<Type> params = {});
    
    bool declare(Scope& scope, const Symbol& symbol, const AST::Node& node);
    Symbol* lookupLocal(Scope& scope, const std::string& name);
    Symbol* lookupLexical(const std::string& name);    // найти переменную в текущей scope и парентских
    Symbol* resolvePath(const std::vector<std::string>& path, const AST::Node& node);
    Symbol* resolvePathFromScope(Scope& start, const std::vector<std::string>& path, const AST::Node& node);

    // Анализ разных элементов AST
    void analyzeDecl(AST::Decl& decl);           // объявление
    void analyzeNamespaceDecl(AST::NamespaceDecl& decl);
    void analyzeTypeAliasDecl(AST::TypeAliasDecl& decl);
    void analyzeStructDecl(AST::StructDecl& decl);
    void analyzeFunctionDecl(AST::FunctionDecl& decl);
    Type resolveType(AST::TypeExpr& typeExpr);
    Type resolveNamedType(AST::NamedType& typeExpr);
    Flow analyzeBlock(AST::BlockStmt& block, bool createScope);
    Flow analyzeStmt(AST::Stmt& stmt);           // оператор
    Flow analyzeIf(AST::IfStmt& stmt);
    Flow analyzeWhile(AST::WhileStmt& stmt);
    Flow analyzeExprStmt(AST::ExprStmt& stmt);
    Type analyzeExpr(AST::Expr& expr, const std::optional<Type>& expected = std::nullopt);  // выражение
    Type analyzeNameExpr(AST::NameExpr& expr);
    Type analyzeArrayLiteral(AST::ArrayLiteralExpr& expr, const std::optional<Type>& expected);
    Type analyzeStructLiteral(AST::StructLiteralExpr& expr);
    Type analyzeUnary(AST::UnaryExpr& expr);
    Type analyzeBinary(AST::BinaryExpr& expr);
    Type analyzeCast(AST::CastExpr& expr);
    Type analyzeCall(AST::CallExpr& expr);
    Type analyzeField(AST::FieldExpr& expr);
    Type analyzeIndex(AST::IndexExpr& expr);
    LValueInfo analyzeLValue(AST::Expr& expr);

    // Проверки типов
    bool checkAssignable(const Type& lhs, const Type& rhs, const AST::Node& node);  // учитывают неявное преобразование
    bool canCast(const Type& from, const Type& to) const;  // можно ли снижать?
    bool isPrintable(const Type& type) const;
    bool isTerminatingCall(const AST::Expr& expr) const;
};

// Форматировать ошибку красиво
std::string formatDiagnostic(const Diagnostic& diagnostic);

} // namespace Semantic
