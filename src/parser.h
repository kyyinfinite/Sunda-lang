#pragma once

#include <string>
#include <vector>

#include "ast.h"
#include "token.h"

namespace sunda {

class Parser {
public:
    explicit Parser(std::vector<Token> tokenList);
    Program parse();

private:
    enum Precedence {
        PrecNone,
        PrecAssignment,
        PrecOr,
        PrecAnd,
        PrecNot,
        PrecEquality,
        PrecComparison,
        PrecTerm,
        PrecFactor,
        PrecUnary,
        PrecPower,
        PrecCall,
    };

    const Token& peek() const { return tokens[position]; }
    bool check(TokenType type) const { return peek().type == type; }
    const Token& advance();
    bool match(TokenType type);
    const Token& consume(TokenType type, const char* message);
    void skipNewlines();
    [[noreturn]] void fail(const Token& token, std::string message) const;

    StmtPtr statement();
    StmtPtr functionStatement();
    StmtPtr classStatement();
    StmtPtr importStatement();
    StmtPtr tryStatement();
    StmtPtr throwStatement();
    StmtPtr returnStatement();
    StmtPtr expressionStatement();
    StmtPtr blockStatement();
    StmtPtr ifStatement();
    StmtPtr whileStatement();
    StmtPtr forStatement();
    StmtPtr jumpStatement();
    void endStatement();
    void parseParams(FunctionStmt& fn);

    ExprPtr expression();
    ExprPtr parsePrecedence(int minimum);
    ExprPtr prefix();
    ExprPtr infix(ExprPtr left, const Token& op);
    ExprPtr assignment(ExprPtr left, const Token& op);
    ExprPtr finishCall(ExprPtr callee, const Token& open);
    ExprPtr listExpr(const Token& open);
    ExprPtr braceExpr(const Token& open);
    ExprPtr parenExpr(const Token& open);
    ExprPtr indexOrSlice(ExprPtr object, const Token& open);
    std::vector<ExprPtr> argumentList();
    static int infixPrecedence(TokenType type);

    std::vector<Token> tokens;
    size_t position = 0;
};

}
