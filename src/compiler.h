#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ast.h"
#include "chunk.h"
#include "memory.h"

namespace sunda {

class Compiler {
public:
    Compiler(Heap& heapRef, bool replEchoMode = false);
    ObjFunction* compile(const Program& program, std::string_view source, const std::string& name);

private:
    struct LoopContext {
        std::vector<size_t> breakJumps;
        std::vector<size_t> continueJumps;
    };

    struct FnState {
        Chunk chunk;
        std::vector<LoopContext> loops;
        FunctionStmt* fn = nullptr;
        bool isTopLevel = false;
    };

    void statement(const Stmt& stmt);
    void expression(const Expr& expr);
    void binary(const BinaryExpr& expr);
    void logical(const LogicalExpr& expr);
    void assign(const AssignExpr& expr);
    void callArguments(const CallExpr& expr);
    void ifStatement(const IfStmt& stmt);
    void whileStatement(const WhileStmt& stmt);
    void forStatement(const ForStmt& stmt);
    void functionStatement(const FunctionStmt& stmt);
    void classStatement(const ClassStmt& stmt);
    void tryStatement(const TryStmt& stmt);
    void storeVariable(const std::string& name, VarScope scope, uint16_t slot, bool declares);
    void loadVariable(const std::string& name, VarScope scope, uint16_t slot);
    ObjFunction* compileFunction(FunctionStmt& fn, bool isMethod);
    void emitPrologue(FunctionStmt& fn);
    void emitPops(size_t count, int line);
    void emitCloses(size_t count, int line);
    void emitShort(uint16_t value, int line);
    size_t emitJump(OpCode op, int line);
    void patchJump(size_t operandOffset, int line);
    void patchAll(const std::vector<size_t>& jumps, int line);
    void emitLoop(size_t loopStart, int line);
    void emitByte(uint8_t byte, int line);
    void emitOp(OpCode op, int line);
    void emitOperandOp(OpCode op, uint16_t operand, int line);
    uint16_t makeConstant(Value value, int line);
    uint16_t identifierConstant(const std::string& name, int line);
    bool foldBinary(const BinaryExpr& expr);
    bool foldUnary(const UnaryExpr& expr);
    bool isLastStatement(const Stmt& stmt) const;

    Chunk& chunk() { return states.back().chunk; }

    Heap& heap;
    std::vector<FnState> states;
    const Program* topLevelProgram = nullptr;
    std::string_view sourceView;
    bool replEcho = false;
    int currentLine = 1;
};

}
