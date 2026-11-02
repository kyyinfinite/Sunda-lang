#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "token.h"

namespace sunda {

enum class ExprKind {
    Number,
    String,
    Literal,
    Variable,
    Unary,
    Binary,
    Logical,
    Assign,
    Call,
    Property,
    Index,
    Slice,
    Super,
    List,
    Map,
    Set,
    Tuple,
};

enum class VarScope {
    Global,
    Local,
    Upvalue,
};

struct UpvalueDesc {
    bool isLocal = false;
    uint16_t index = 0;
};

struct Expr {
    ExprKind kind;
    int line;
    int column;

    Expr(ExprKind kindValue, int lineValue, int columnValue)
        : kind(kindValue), line(lineValue), column(columnValue) {}
    virtual ~Expr() = default;
};

using ExprPtr = std::unique_ptr<Expr>;

struct NumberExpr : Expr {
    double value;

    NumberExpr(double numberValue, int lineValue, int columnValue)
        : Expr(ExprKind::Number, lineValue, columnValue), value(numberValue) {}
};

struct StringExpr : Expr {
    std::string value;

    StringExpr(std::string text, int lineValue, int columnValue)
        : Expr(ExprKind::String, lineValue, columnValue), value(std::move(text)) {}
};

struct LiteralExpr : Expr {
    TokenType which;

    LiteralExpr(TokenType literal, int lineValue, int columnValue)
        : Expr(ExprKind::Literal, lineValue, columnValue), which(literal) {}
};

struct VariableExpr : Expr {
    std::string name;
    VarScope scope = VarScope::Global;
    uint16_t slot = 0;

    VariableExpr(std::string identifier, int lineValue, int columnValue)
        : Expr(ExprKind::Variable, lineValue, columnValue), name(std::move(identifier)) {}
};

struct UnaryExpr : Expr {
    TokenType op;
    ExprPtr operand;

    UnaryExpr(TokenType opType, ExprPtr inner, int lineValue, int columnValue)
        : Expr(ExprKind::Unary, lineValue, columnValue), op(opType), operand(std::move(inner)) {}
};

struct BinaryExpr : Expr {
    TokenType op;
    ExprPtr left;
    ExprPtr right;

    BinaryExpr(TokenType opType, ExprPtr leftSide, ExprPtr rightSide, int lineValue, int columnValue)
        : Expr(ExprKind::Binary, lineValue, columnValue),
          op(opType),
          left(std::move(leftSide)),
          right(std::move(rightSide)) {}
};

struct LogicalExpr : Expr {
    TokenType op;
    ExprPtr left;
    ExprPtr right;

    LogicalExpr(TokenType opType, ExprPtr leftSide, ExprPtr rightSide, int lineValue, int columnValue)
        : Expr(ExprKind::Logical, lineValue, columnValue),
          op(opType),
          left(std::move(leftSide)),
          right(std::move(rightSide)) {}
};

struct AssignExpr : Expr {
    ExprPtr target;
    ExprPtr value;
    VarScope scope = VarScope::Global;
    uint16_t slot = 0;
    bool declares = false;
    bool targetIsCompound = false;
    TokenType compoundOp = TokenType::Equal;

    AssignExpr(ExprPtr assignmentTarget, ExprPtr assigned, int lineValue, int columnValue)
        : Expr(ExprKind::Assign, lineValue, columnValue),
          target(std::move(assignmentTarget)),
          value(std::move(assigned)) {}
};

struct CallExpr : Expr {
    ExprPtr callee;
    std::vector<ExprPtr> arguments;

    CallExpr(ExprPtr target, std::vector<ExprPtr> args, int lineValue, int columnValue)
        : Expr(ExprKind::Call, lineValue, columnValue),
          callee(std::move(target)),
          arguments(std::move(args)) {}
};

struct PropertyExpr : Expr {
    ExprPtr object;
    std::string name;

    PropertyExpr(ExprPtr receiver, std::string propertyName, int lineValue, int columnValue)
        : Expr(ExprKind::Property, lineValue, columnValue),
          object(std::move(receiver)),
          name(std::move(propertyName)) {}
};

struct IndexExpr : Expr {
    ExprPtr object;
    ExprPtr index;

    IndexExpr(ExprPtr receiver, ExprPtr position, int lineValue, int columnValue)
        : Expr(ExprKind::Index, lineValue, columnValue),
          object(std::move(receiver)),
          index(std::move(position)) {}
};

struct SliceExpr : Expr {
    ExprPtr object;
    ExprPtr start;
    ExprPtr end;
    bool hasStart = false;
    bool hasEnd = false;

    SliceExpr(ExprPtr receiver, int lineValue, int columnValue)
        : Expr(ExprKind::Slice, lineValue, columnValue), object(std::move(receiver)) {}
};

struct SuperExpr : Expr {
    std::string method;

    SuperExpr(std::string methodName, int lineValue, int columnValue)
        : Expr(ExprKind::Super, lineValue, columnValue), method(std::move(methodName)) {}
};

struct ListExpr : Expr {
    std::vector<ExprPtr> elements;

    ListExpr(std::vector<ExprPtr> items, int lineValue, int columnValue)
        : Expr(ExprKind::List, lineValue, columnValue), elements(std::move(items)) {}
};

struct MapEntry {
    ExprPtr key;
    ExprPtr value;
};

struct MapExpr : Expr {
    std::vector<MapEntry> entries;

    MapExpr(int lineValue, int columnValue) : Expr(ExprKind::Map, lineValue, columnValue) {}
};

struct SetExpr : Expr {
    std::vector<ExprPtr> elements;

    SetExpr(int lineValue, int columnValue) : Expr(ExprKind::Set, lineValue, columnValue) {}
};

struct TupleExpr : Expr {
    std::vector<ExprPtr> elements;

    TupleExpr(std::vector<ExprPtr> items, int lineValue, int columnValue)
        : Expr(ExprKind::Tuple, lineValue, columnValue), elements(std::move(items)) {}
};

enum class StmtKind {
    Expression,
    Block,
    If,
    While,
    For,
    Break,
    Continue,
    Function,
    Return,
    Class,
    Import,
    Try,
    Throw,
};

struct Stmt {
    StmtKind kind;
    int line;

    Stmt(StmtKind kindValue, int lineValue) : kind(kindValue), line(lineValue) {}
    virtual ~Stmt() = default;
};

using StmtPtr = std::unique_ptr<Stmt>;

struct ExpressionStmt : Stmt {
    ExprPtr expression;

    ExpressionStmt(ExprPtr expr, int lineValue)
        : Stmt(StmtKind::Expression, lineValue), expression(std::move(expr)) {}
};

struct BlockStmt : Stmt {
    std::vector<StmtPtr> statements;
    size_t localCount = 0;
    bool closeUpvalues = false;

    BlockStmt(std::vector<StmtPtr> body, int lineValue)
        : Stmt(StmtKind::Block, lineValue), statements(std::move(body)) {}
};

struct IfStmt : Stmt {
    ExprPtr condition;
    StmtPtr thenBranch;
    StmtPtr elseBranch;

    IfStmt(ExprPtr test, StmtPtr thenPart, StmtPtr elsePart, int lineValue)
        : Stmt(StmtKind::If, lineValue),
          condition(std::move(test)),
          thenBranch(std::move(thenPart)),
          elseBranch(std::move(elsePart)) {}
};

struct WhileStmt : Stmt {
    ExprPtr condition;
    StmtPtr body;
    bool breakCaptured = false;
    bool continueCaptured = false;

    WhileStmt(ExprPtr test, StmtPtr loopBody, int lineValue)
        : Stmt(StmtKind::While, lineValue), condition(std::move(test)), body(std::move(loopBody)) {}
};

struct ForStmt : Stmt {
    std::string variable;
    ExprPtr iterable;
    StmtPtr body;
    uint16_t hiddenSlot = 0;
    bool varCaptured = false;
    bool breakCaptured = false;
    bool continueCaptured = false;

    ForStmt(std::string name, ExprPtr source, StmtPtr loopBody, int lineValue)
        : Stmt(StmtKind::For, lineValue),
          variable(std::move(name)),
          iterable(std::move(source)),
          body(std::move(loopBody)) {}
};

struct BreakStmt : Stmt {
    size_t popCount = 0;
    bool closeUpvalues = false;

    explicit BreakStmt(int lineValue) : Stmt(StmtKind::Break, lineValue) {}
};

struct ContinueStmt : Stmt {
    size_t popCount = 0;
    bool closeUpvalues = false;

    explicit ContinueStmt(int lineValue) : Stmt(StmtKind::Continue, lineValue) {}
};

struct Param {
    std::string name;
    ExprPtr defaultValue;
    int line = 1;
};

struct FunctionStmt : Stmt {
    std::string name;
    std::vector<Param> params;
    int restIndex = -1;
    std::string restName;
    StmtPtr body;
    uint16_t localCount = 0;
    uint16_t upvalueCount = 0;
    std::vector<UpvalueDesc> upvalueDescs;
    VarScope scope = VarScope::Global;
    uint16_t slot = 0;
    bool declares = false;

    FunctionStmt(std::string functionName, int lineValue)
        : Stmt(StmtKind::Function, lineValue), name(std::move(functionName)) {}
};

struct ReturnStmt : Stmt {
    ExprPtr value;

    explicit ReturnStmt(int lineValue) : Stmt(StmtKind::Return, lineValue) {}
};

struct ClassStmt : Stmt {
    std::string name;
    std::string superclass;
    bool hasSuperclass = false;
    VarScope superScope = VarScope::Global;
    uint16_t superSlot = 0;
    std::vector<StmtPtr> methods;
    VarScope scope = VarScope::Global;
    uint16_t slot = 0;
    bool declares = false;

    ClassStmt(std::string className, int lineValue)
        : Stmt(StmtKind::Class, lineValue), name(std::move(className)) {}
};

struct ImportStmt : Stmt {
    std::string name;
    VarScope scope = VarScope::Global;
    uint16_t slot = 0;

    ImportStmt(std::string moduleName, int lineValue)
        : Stmt(StmtKind::Import, lineValue), name(std::move(moduleName)) {}
};

struct TryStmt : Stmt {
    StmtPtr body;
    std::string catchVar;
    bool hasCatchVar = false;
    uint16_t catchSlot = 0;
    bool closeUpvalues = false;
    StmtPtr handler;

    TryStmt(int lineValue) : Stmt(StmtKind::Try, lineValue) {}
};

struct ThrowStmt : Stmt {
    ExprPtr value;

    ThrowStmt(ExprPtr thrown, int lineValue)
        : Stmt(StmtKind::Throw, lineValue), value(std::move(thrown)) {}
};

using Program = std::vector<StmtPtr>;

}
