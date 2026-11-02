#include "compiler.h"

#include <cmath>

namespace sunda {

namespace {

OpCode compoundOpCode(TokenType type) {
    switch (type) {
        case TokenType::Plus: return OpCode::Add;
        case TokenType::Minus: return OpCode::Subtract;
        case TokenType::Star: return OpCode::Multiply;
        default: return OpCode::Divide;
    }
}

}

Compiler::Compiler(Heap& heapRef, bool replEchoMode)
    : heap(heapRef), replEcho(replEchoMode) {}

ObjFunction* Compiler::compile(const Program& program, std::string_view source, const std::string& name) {
    topLevelProgram = &program;
    sourceView = source;
    states.emplace_back();
    states.back().isTopLevel = true;
    for (const StmtPtr& stmt : program) statement(*stmt);
    emitOp(OpCode::Nil, currentLine);
    emitOp(OpCode::Return, currentLine);
    ObjFunction* function = allocateObject<ObjFunction>(heap, ObjType::Function);
    function->name = copyString(heap, name);
    function->source = source;
    function->chunk = std::move(states.back().chunk);
    states.pop_back();
    return function;
}

void Compiler::emitByte(uint8_t byte, int line) {
    currentLine = line;
    chunk().write(byte, line);
}

void Compiler::emitOp(OpCode op, int line) { emitByte(static_cast<uint8_t>(op), line); }

void Compiler::emitOperandOp(OpCode op, uint16_t operand, int line) {
    emitOp(op, line);
    emitByte(static_cast<uint8_t>(operand >> 8), line);
    emitByte(static_cast<uint8_t>(operand & 0xff), line);
}

void Compiler::emitShort(uint16_t value, int line) {
    emitByte(static_cast<uint8_t>(value >> 8), line);
    emitByte(static_cast<uint8_t>(value & 0xff), line);
}

void Compiler::emitPops(size_t count, int line) {
    while (count > 0) {
        size_t batch = count > 255 ? 255 : count;
        if (batch == 1) {
            emitOp(OpCode::Pop, line);
        } else {
            emitOp(OpCode::PopN, line);
            emitByte(static_cast<uint8_t>(batch), line);
        }
        count -= batch;
    }
}

void Compiler::emitCloses(size_t count, int line) {
    if (count == 0 || count > 255) return;
    emitOp(OpCode::CloseUpvalueN, line);
    emitByte(static_cast<uint8_t>(count), line);
}

size_t Compiler::emitJump(OpCode op, int line) {
    emitOp(op, line);
    emitByte(0xff, line);
    emitByte(0xff, line);
    return chunk().code.size() - 2;
}

void Compiler::patchJump(size_t operandOffset, int line) {
    size_t distance = chunk().code.size() - operandOffset - 2;
    if (distance > 0xffff) throw SyntaxError{line, 1, "lompatan jauh teuing"};
    chunk().code[operandOffset] = static_cast<uint8_t>(distance >> 8);
    chunk().code[operandOffset + 1] = static_cast<uint8_t>(distance & 0xff);
}

void Compiler::patchAll(const std::vector<size_t>& jumps, int line) {
    for (size_t jump : jumps) patchJump(jump, line);
}

void Compiler::emitLoop(size_t loopStart, int line) {
    emitOp(OpCode::Loop, line);
    size_t distance = chunk().code.size() - loopStart + 2;
    if (distance > 0xffff) throw SyntaxError{line, 1, "lompatan jauh teuing"};
    emitShort(static_cast<uint16_t>(distance), line);
}

uint16_t Compiler::makeConstant(Value value, int line) {
    size_t index = chunk().addConstant(value);
    if (index > 0xffff) throw SyntaxError{line, 1, "loba teuing konstanta"};
    return static_cast<uint16_t>(index);
}

uint16_t Compiler::identifierConstant(const std::string& name, int line) {
    return makeConstant(objValue(copyString(heap, name)), line);
}

bool Compiler::foldBinary(const BinaryExpr& expr) {
    if (expr.left->kind != ExprKind::Number || expr.right->kind != ExprKind::Number) return false;
    double a = static_cast<const NumberExpr&>(*expr.left).value;
    double b = static_cast<const NumberExpr&>(*expr.right).value;
    double result = 0;
    switch (expr.op) {
        case TokenType::Plus: result = a + b; break;
        case TokenType::Minus: result = a - b; break;
        case TokenType::Star: result = a * b; break;
        case TokenType::Slash:
            if (b == 0) return false;
            result = a / b;
            break;
        case TokenType::Percent:
            if (b == 0) return false;
            result = std::fmod(a, b);
            break;
        case TokenType::StarStar: result = std::pow(a, b); break;
        default: return false;
    }
    emitOperandOp(OpCode::Constant, makeConstant(numberValue(result), expr.line), expr.line);
    return true;
}

bool Compiler::foldUnary(const UnaryExpr& expr) {
    if (expr.op != TokenType::Minus || expr.operand->kind != ExprKind::Number) return false;
    double value = -static_cast<const NumberExpr&>(*expr.operand).value;
    emitOperandOp(OpCode::Constant, makeConstant(numberValue(value), expr.line), expr.line);
    return true;
}

bool Compiler::isLastStatement(const Stmt& stmt) const {
    if (states.size() != 1 || states.back().fn != nullptr) return false;
    if (topLevelProgram == nullptr || topLevelProgram->empty()) return false;
    return topLevelProgram->back().get() == &stmt;
}

void Compiler::statement(const Stmt& stmt) {
    switch (stmt.kind) {
        case StmtKind::Expression: {
            const ExpressionStmt& node = static_cast<const ExpressionStmt&>(stmt);
            bool isLast = this->isLastStatement(stmt);
            expression(*node.expression);
            bool keepsValue = node.expression->kind == ExprKind::Assign &&
                              static_cast<const AssignExpr&>(*node.expression).declares;
            if (isLast && replEcho) {
                emitOp(OpCode::Echo, stmt.line);
            } else if (!keepsValue) {
                emitOp(OpCode::Pop, stmt.line);
            }
            break;
        }
        case StmtKind::Block: {
            const BlockStmt& node = static_cast<const BlockStmt&>(stmt);
            for (const StmtPtr& inner : node.statements) statement(*inner);
            emitCloses(node.localCount, stmt.line);
            emitPops(node.localCount, stmt.line);
            break;
        }
        case StmtKind::If:
            ifStatement(static_cast<const IfStmt&>(stmt));
            break;
        case StmtKind::While:
            whileStatement(static_cast<const WhileStmt&>(stmt));
            break;
        case StmtKind::For:
            forStatement(static_cast<const ForStmt&>(stmt));
            break;
        case StmtKind::Break: {
            const BreakStmt& node = static_cast<const BreakStmt&>(stmt);
            if (node.closeUpvalues && node.popCount > 0) emitCloses(node.popCount, stmt.line);
            emitPops(node.popCount, stmt.line);
            states.back().loops.back().breakJumps.push_back(emitJump(OpCode::Jump, stmt.line));
            break;
        }
        case StmtKind::Continue: {
            const ContinueStmt& node = static_cast<const ContinueStmt&>(stmt);
            if (node.closeUpvalues && node.popCount > 0) emitCloses(node.popCount, stmt.line);
            emitPops(node.popCount, stmt.line);
            states.back().loops.back().continueJumps.push_back(emitJump(OpCode::Jump, stmt.line));
            break;
        }
        case StmtKind::Function:
            functionStatement(static_cast<const FunctionStmt&>(stmt));
            break;
        case StmtKind::Return: {
            const ReturnStmt& node = static_cast<const ReturnStmt&>(stmt);
            if (node.value) expression(*node.value);
            else emitOp(OpCode::Nil, stmt.line);
            emitOp(OpCode::Return, stmt.line);
            break;
        }
        case StmtKind::Class:
            classStatement(static_cast<const ClassStmt&>(stmt));
            break;
        case StmtKind::Import: {
            const ImportStmt& node = static_cast<const ImportStmt&>(stmt);
            emitOp(OpCode::Import, stmt.line);
            emitShort(identifierConstant(node.name, stmt.line), stmt.line);
            emitByte(node.scope == VarScope::Local ? 1 : 0, stmt.line);
            emitShort(node.slot, stmt.line);
            break;
        }
        case StmtKind::Try:
            tryStatement(static_cast<const TryStmt&>(stmt));
            break;
        case StmtKind::Throw: {
            const ThrowStmt& node = static_cast<const ThrowStmt&>(stmt);
            expression(*node.value);
            emitOp(OpCode::Throw, stmt.line);
            break;
        }
    }
}

void Compiler::storeVariable(const std::string& name, VarScope scope, uint16_t slot, bool declares) {
    int line = currentLine;
    switch (scope) {
        case VarScope::Global:
            emitOperandOp(OpCode::SetGlobal, identifierConstant(name, line), line);
            break;
        case VarScope::Local:
            if (!declares) emitOperandOp(OpCode::SetLocal, slot, line);
            break;
        case VarScope::Upvalue:
            emitOperandOp(OpCode::SetUpvalue, slot, line);
            break;
    }
}

void Compiler::loadVariable(const std::string& name, VarScope scope, uint16_t slot) {
    int line = currentLine;
    switch (scope) {
        case VarScope::Local:
            emitOperandOp(OpCode::GetLocal, slot, line);
            break;
        case VarScope::Upvalue:
            emitOperandOp(OpCode::GetUpvalue, slot, line);
            break;
        case VarScope::Global:
            emitOperandOp(OpCode::GetGlobal, identifierConstant(name, line), line);
            break;
    }
}

void Compiler::ifStatement(const IfStmt& stmt) {
    expression(*stmt.condition);
    size_t thenJump = emitJump(OpCode::JumpIfFalse, stmt.line);
    emitOp(OpCode::Pop, stmt.line);
    statement(*stmt.thenBranch);
    size_t elseJump = emitJump(OpCode::Jump, stmt.line);
    patchJump(thenJump, stmt.line);
    emitOp(OpCode::Pop, stmt.line);
    if (stmt.elseBranch) statement(*stmt.elseBranch);
    patchJump(elseJump, stmt.line);
}

void Compiler::whileStatement(const WhileStmt& stmt) {
    size_t loopStart = chunk().code.size();
    expression(*stmt.condition);
    size_t exitJump = emitJump(OpCode::JumpIfFalse, stmt.line);
    emitOp(OpCode::Pop, stmt.line);
    states.back().loops.emplace_back();
    statement(*stmt.body);
    patchAll(states.back().loops.back().continueJumps, stmt.line);
    emitLoop(loopStart, stmt.line);
    patchJump(exitJump, stmt.line);
    emitOp(OpCode::Pop, stmt.line);
    patchAll(states.back().loops.back().breakJumps, stmt.line);
    states.back().loops.pop_back();
}

void Compiler::forStatement(const ForStmt& stmt) {
    bool topLevel = states.back().isTopLevel;
    if (topLevel) {
        expression(*stmt.iterable);
        emitOperandOp(OpCode::Constant, makeConstant(numberValue(0), stmt.line), stmt.line);
        size_t loopStart = chunk().code.size();
        emitOperandOp(OpCode::IterNext, stmt.hiddenSlot, stmt.line);
        emitShort(0xffff, stmt.line);
        size_t exitJump = chunk().code.size() - 2;
        states.back().loops.emplace_back();
        statement(*stmt.body);
        patchAll(states.back().loops.back().continueJumps, stmt.line);
        if (stmt.varCaptured) {
            emitOp(OpCode::CloseUpvalueN, stmt.line);
            emitByte(1, stmt.line);
        }
        emitOp(OpCode::Pop, stmt.line);
        emitLoop(loopStart, stmt.line);
        patchJump(exitJump, stmt.line);
        patchAll(states.back().loops.back().breakJumps, stmt.line);
        states.back().loops.pop_back();
        emitPops(2, stmt.line);
    } else {
        uint16_t iterableSlot = stmt.hiddenSlot;
        uint16_t varSlot = static_cast<uint16_t>(stmt.hiddenSlot + 2);
        expression(*stmt.iterable);
        emitOperandOp(OpCode::SetLocal, iterableSlot, stmt.line);
        emitOp(OpCode::Pop, stmt.line);
        emitOperandOp(OpCode::Constant, makeConstant(numberValue(0), stmt.line), stmt.line);
        emitOperandOp(OpCode::SetLocal, static_cast<uint16_t>(iterableSlot + 1), stmt.line);
        emitOp(OpCode::Pop, stmt.line);
        size_t loopStart = chunk().code.size();
        emitOperandOp(OpCode::IterNext, iterableSlot, stmt.line);
        emitShort(0xffff, stmt.line);
        size_t exitJump = chunk().code.size() - 2;
        states.back().loops.emplace_back();
        emitOperandOp(OpCode::SetLocal, varSlot, stmt.line);
        emitOp(OpCode::Pop, stmt.line);
        statement(*stmt.body);
        patchAll(states.back().loops.back().continueJumps, stmt.line);
        if (stmt.varCaptured) {
            emitOp(OpCode::CloseUpvalueFrom, stmt.line);
            emitShort(varSlot, stmt.line);
        }
        emitLoop(loopStart, stmt.line);
        patchJump(exitJump, stmt.line);
        patchAll(states.back().loops.back().breakJumps, stmt.line);
        states.back().loops.pop_back();
    }
}

ObjFunction* Compiler::compileFunction(FunctionStmt& fn, bool isMethod) {
    states.emplace_back();
    states.back().fn = &fn;

    emitPrologue(fn);
    const BlockStmt& body = static_cast<const BlockStmt&>(*fn.body);
    for (const StmtPtr& inner : body.statements) statement(*inner);
    emitOp(OpCode::Nil, fn.line);
    emitOp(OpCode::Return, fn.line);

    ObjFunction* function = allocateObject<ObjFunction>(heap, ObjType::Function);
    function->name = copyString(heap, fn.name);
    function->source = sourceView;
    function->isMethod = isMethod;
    function->upvalueCount = static_cast<int>(fn.upvalueDescs.size());
    function->slotCount = static_cast<int>(fn.localCount) + 1;
    size_t userParams = fn.params.size();
    function->paramSlots = static_cast<int>(userParams) + (isMethod ? 1 : 0);
    int firstDefault = -1;
    for (size_t index = 0; index < fn.params.size(); ++index) {
        if (fn.params[index].defaultValue) {
            firstDefault = static_cast<int>(index);
            break;
        }
    }
    int required = firstDefault >= 0 ? firstDefault : static_cast<int>(userParams);
    function->arityMin = required + (isMethod ? 1 : 0);
    function->arityMax = fn.restIndex >= 0
                             ? -1
                             : static_cast<int>(userParams) + (isMethod ? 1 : 0);
    function->restSlot = fn.restIndex >= 0 ? static_cast<int>(userParams) + 1 : -1;
    function->firstDefaultSlot = firstDefault >= 0 ? firstDefault + 1 : -1;
    function->chunk = std::move(states.back().chunk);
    states.pop_back();
    return function;
}

void Compiler::emitPrologue(FunctionStmt& fn) {
    for (size_t index = 0; index < fn.params.size(); ++index) {
        const Param& param = fn.params[index];
        if (!param.defaultValue) continue;
        uint16_t slot = static_cast<uint16_t>(index + 1);
        emitOperandOp(OpCode::GetLocal, slot, fn.line);
        emitOp(OpCode::Not, fn.line);
        emitOp(OpCode::Not, fn.line);
        size_t skip = emitJump(OpCode::JumpIfTrue, fn.line);
        expression(*param.defaultValue);
        emitOperandOp(OpCode::SetLocal, slot, fn.line);
        emitOp(OpCode::Pop, fn.line);
        patchJump(skip, fn.line);
    }
}

void Compiler::functionStatement(const FunctionStmt& stmt) {
    FunctionStmt& mutableStmt = const_cast<FunctionStmt&>(stmt);
    ObjFunction* function = compileFunction(mutableStmt, false);
    int line = stmt.line;
    emitOperandOp(OpCode::Closure, makeConstant(objValue(function), line), line);
    emitByte(static_cast<uint8_t>(stmt.upvalueDescs.size()), line);
    for (const UpvalueDesc& desc : stmt.upvalueDescs) {
        emitByte(desc.isLocal ? 1 : 0, line);
        emitByte(static_cast<uint8_t>(desc.index), line);
    }
    storeVariable(stmt.name, stmt.scope, stmt.slot, stmt.declares);
    if (!(stmt.scope == VarScope::Local && stmt.declares)) emitOp(OpCode::Pop, line);
    else currentLine = line;
}

void Compiler::classStatement(const ClassStmt& stmt) {
    int line = stmt.line;
    if (stmt.hasSuperclass) {
        loadVariable(stmt.superclass, stmt.superScope, stmt.superSlot);
    } else {
        emitOp(OpCode::Nil, line);
    }
    for (const StmtPtr& methodPtr : stmt.methods) {
        FunctionStmt& method = *static_cast<FunctionStmt*>(methodPtr.get());
        ObjFunction* function = compileFunction(method, true);
        emitOperandOp(OpCode::Closure, makeConstant(objValue(function), line), line);
        emitByte(static_cast<uint8_t>(method.upvalueDescs.size()), line);
        for (const UpvalueDesc& desc : method.upvalueDescs) {
            emitByte(desc.isLocal ? 1 : 0, line);
            emitByte(static_cast<uint8_t>(desc.index), line);
        }
    }
    emitOp(OpCode::DefineClass, line);
    emitShort(identifierConstant(stmt.name, line), line);
    emitByte(static_cast<uint8_t>(stmt.methods.size()), line);
    storeVariable(stmt.name, stmt.scope, stmt.slot, stmt.declares);
    if (!(stmt.scope == VarScope::Local && stmt.declares)) emitOp(OpCode::Pop, line);
    else currentLine = line;
}

void Compiler::tryStatement(const TryStmt& stmt) {
    int line = stmt.line;
    bool inFunctionScope = !states.back().isTopLevel;
    emitOp(OpCode::Try, line);
    emitByte(0xff, line);
    emitByte(0xff, line);
    size_t handlerOffset = chunk().code.size() - 2;

    statement(*stmt.body);
    emitOp(OpCode::TryEnd, line);
    size_t endJump = emitJump(OpCode::Jump, line);
    patchJump(handlerOffset, line);
    if (inFunctionScope && stmt.hasCatchVar) {
        emitOperandOp(OpCode::SetLocal, stmt.catchSlot, line);
        emitOp(OpCode::Pop, line);
    }
    statement(*stmt.handler);
    if (!inFunctionScope && stmt.hasCatchVar) {
        if (stmt.closeUpvalues) {
            emitOp(OpCode::CloseUpvalueN, line);
            emitByte(1, line);
        }
        emitOp(OpCode::Pop, line);
    } else if (inFunctionScope && stmt.hasCatchVar && stmt.closeUpvalues) {
        emitOp(OpCode::CloseUpvalueFrom, line);
        emitShort(stmt.catchSlot, line);
    }
    patchJump(endJump, line);
}

void Compiler::callArguments(const CallExpr& expr) {
    if (expr.arguments.size() > 255) throw SyntaxError{expr.line, expr.column, "argumen kabeh teuing"};
    if (expr.callee->kind == ExprKind::Property) {
        const PropertyExpr& property = static_cast<const PropertyExpr&>(*expr.callee);
        expression(*property.object);
        for (const ExprPtr& argument : expr.arguments) expression(*argument);
        emitOp(OpCode::Invoke, expr.line);
        emitShort(identifierConstant(property.name, expr.line), expr.line);
        emitByte(static_cast<uint8_t>(expr.arguments.size()), expr.line);
        return;
    }
    if (expr.callee->kind == ExprKind::Super) {
        const SuperExpr& super = static_cast<const SuperExpr&>(*expr.callee);
        emitOperandOp(OpCode::GetLocal, 0, expr.line);
        for (const ExprPtr& argument : expr.arguments) expression(*argument);
        emitOp(OpCode::GetSuper, expr.line);
        emitShort(identifierConstant(super.method, expr.line), expr.line);
        emitByte(static_cast<uint8_t>(expr.arguments.size()), expr.line);
        return;
    }
    expression(*expr.callee);
    for (const ExprPtr& argument : expr.arguments) expression(*argument);
    emitOp(OpCode::Call, expr.line);
    emitByte(static_cast<uint8_t>(expr.arguments.size()), expr.line);
}

void Compiler::assign(const AssignExpr& expr) {
    const Expr& target = *expr.target;
    int line = expr.line;
    if (target.kind == ExprKind::Variable) {
        const VariableExpr& node = static_cast<const VariableExpr&>(target);
        if (expr.targetIsCompound) {
            loadVariable(node.name, expr.scope, expr.slot);
            expression(*expr.value);
            emitOp(compoundOpCode(expr.compoundOp), line);
            storeVariable(node.name, expr.scope, expr.slot, false);
        } else {
            expression(*expr.value);
            storeVariable(node.name, expr.scope, expr.slot, expr.declares);
        }
        return;
    }
    if (target.kind == ExprKind::Property) {
        const PropertyExpr& node = static_cast<const PropertyExpr&>(target);
        uint16_t name = identifierConstant(node.name, line);
        if (expr.targetIsCompound) {
            expression(*node.object);
            emitOp(OpCode::Dup, line);
            emitOp(OpCode::GetProperty, line);
            emitShort(name, line);
            expression(*expr.value);
            emitOp(compoundOpCode(expr.compoundOp), line);
            emitOp(OpCode::SetProperty, line);
            emitShort(name, line);
        } else {
            expression(*node.object);
            expression(*expr.value);
            emitOp(OpCode::SetProperty, line);
            emitShort(name, line);
        }
        return;
    }
    if (target.kind == ExprKind::Index) {
        const IndexExpr& node = static_cast<const IndexExpr&>(target);
        if (expr.targetIsCompound) {
            expression(*node.object);
            expression(*node.index);
            emitOp(OpCode::Dup2, line);
            emitOp(OpCode::GetIndex, line);
            expression(*expr.value);
            emitOp(compoundOpCode(expr.compoundOp), line);
            emitOp(OpCode::SetIndex, line);
        } else {
            expression(*node.object);
            expression(*node.index);
            expression(*expr.value);
            emitOp(OpCode::SetIndex, line);
        }
    }
}

void Compiler::expression(const Expr& expr) {
    switch (expr.kind) {
        case ExprKind::Number: {
            const NumberExpr& node = static_cast<const NumberExpr&>(expr);
            emitOperandOp(OpCode::Constant, makeConstant(numberValue(node.value), expr.line), expr.line);
            break;
        }
        case ExprKind::String: {
            const StringExpr& node = static_cast<const StringExpr&>(expr);
            Value text = objValue(copyString(heap, node.value));
            emitOperandOp(OpCode::Constant, makeConstant(text, expr.line), expr.line);
            break;
        }
        case ExprKind::Literal: {
            const LiteralExpr& node = static_cast<const LiteralExpr&>(expr);
            if (node.which == TokenType::Bener) emitOp(OpCode::True, expr.line);
            else if (node.which == TokenType::Salah) emitOp(OpCode::False, expr.line);
            else emitOp(OpCode::Nil, expr.line);
            break;
        }
        case ExprKind::Variable: {
            const VariableExpr& node = static_cast<const VariableExpr&>(expr);
            loadVariable(node.name, node.scope, node.slot);
            break;
        }
        case ExprKind::Unary: {
            const UnaryExpr& node = static_cast<const UnaryExpr&>(expr);
            if (!foldUnary(node)) {
                expression(*node.operand);
                emitOp(node.op == TokenType::Minus ? OpCode::Negate : OpCode::Not, expr.line);
            }
            break;
        }
        case ExprKind::Binary: {
            const BinaryExpr& node = static_cast<const BinaryExpr&>(expr);
            if (!foldBinary(node)) binary(node);
            break;
        }
        case ExprKind::Logical:
            logical(static_cast<const LogicalExpr&>(expr));
            break;
        case ExprKind::Assign:
            assign(static_cast<const AssignExpr&>(expr));
            break;
        case ExprKind::Call:
            callArguments(static_cast<const CallExpr&>(expr));
            break;
        case ExprKind::Property: {
            const PropertyExpr& node = static_cast<const PropertyExpr&>(expr);
            expression(*node.object);
            emitOp(OpCode::GetProperty, expr.line);
            emitShort(identifierConstant(node.name, expr.line), expr.line);
            break;
        }
        case ExprKind::Index: {
            const IndexExpr& node = static_cast<const IndexExpr&>(expr);
            expression(*node.object);
            expression(*node.index);
            emitOp(OpCode::GetIndex, expr.line);
            break;
        }
        case ExprKind::Slice: {
            const SliceExpr& node = static_cast<const SliceExpr&>(expr);
            expression(*node.object);
            if (node.hasStart) expression(*node.start);
            else emitOp(OpCode::Nil, expr.line);
            if (node.hasEnd) expression(*node.end);
            else emitOp(OpCode::Nil, expr.line);
            uint8_t flags = 0;
            if (node.hasStart) flags |= 1;
            if (node.hasEnd) flags |= 2;
            emitOp(OpCode::Slice, expr.line);
            emitByte(flags, expr.line);
            break;
        }
        case ExprKind::Super:
            throw SyntaxError{expr.line, expr.column, "induk kudu dipanggil salaku metoda"};
        case ExprKind::List: {
            const ListExpr& node = static_cast<const ListExpr&>(expr);
            if (node.elements.size() > 0xffff) throw SyntaxError{expr.line, 1, "daptar kabeh teuing"};
            for (const ExprPtr& element : node.elements) expression(*element);
            emitOp(OpCode::List, expr.line);
            emitShort(static_cast<uint16_t>(node.elements.size()), expr.line);
            break;
        }
        case ExprKind::Map: {
            const MapExpr& node = static_cast<const MapExpr&>(expr);
            if (node.entries.size() > 0xffff) throw SyntaxError{expr.line, 1, "kamus kabeh teuing"};
            for (const MapEntry& entry : node.entries) {
                expression(*entry.key);
                expression(*entry.value);
            }
            emitOp(OpCode::Map, expr.line);
            emitShort(static_cast<uint16_t>(node.entries.size()), expr.line);
            break;
        }
        case ExprKind::Set: {
            const SetExpr& node = static_cast<const SetExpr&>(expr);
            if (node.elements.size() > 0xffff) throw SyntaxError{expr.line, 1, "himpunan kabeh teuing"};
            for (const ExprPtr& element : node.elements) expression(*element);
            emitOp(OpCode::Set, expr.line);
            emitShort(static_cast<uint16_t>(node.elements.size()), expr.line);
            break;
        }
        case ExprKind::Tuple: {
            const TupleExpr& node = static_cast<const TupleExpr&>(expr);
            if (node.elements.size() > 0xffff) throw SyntaxError{expr.line, 1, "tuple kabeh teuing"};
            for (const ExprPtr& element : node.elements) expression(*element);
            emitOp(OpCode::Tuple, expr.line);
            emitShort(static_cast<uint16_t>(node.elements.size()), expr.line);
            break;
        }
    }
}

void Compiler::binary(const BinaryExpr& expr) {
    expression(*expr.left);
    expression(*expr.right);
    int line = expr.line;
    switch (expr.op) {
        case TokenType::Plus: emitOp(OpCode::Add, line); break;
        case TokenType::Minus: emitOp(OpCode::Subtract, line); break;
        case TokenType::Star: emitOp(OpCode::Multiply, line); break;
        case TokenType::Slash: emitOp(OpCode::Divide, line); break;
        case TokenType::Percent: emitOp(OpCode::Modulo, line); break;
        case TokenType::StarStar: emitOp(OpCode::Power, line); break;
        case TokenType::EqualEqual: emitOp(OpCode::Equal, line); break;
        case TokenType::BangEqual:
            emitOp(OpCode::Equal, line);
            emitOp(OpCode::Not, line);
            break;
        case TokenType::Greater: emitOp(OpCode::Greater, line); break;
        case TokenType::GreaterEqual:
            emitOp(OpCode::Less, line);
            emitOp(OpCode::Not, line);
            break;
        case TokenType::Less: emitOp(OpCode::Less, line); break;
        case TokenType::LessEqual:
            emitOp(OpCode::Greater, line);
            emitOp(OpCode::Not, line);
            break;
        default:
            break;
    }
}

void Compiler::logical(const LogicalExpr& expr) {
    expression(*expr.left);
    size_t endJump = emitJump(expr.op == TokenType::Jeung ? OpCode::JumpIfFalse : OpCode::JumpIfTrue, expr.line);
    emitOp(OpCode::Pop, expr.line);
    expression(*expr.right);
    patchJump(endJump, expr.line);
}

}
