#include "resolver.h"

namespace sunda {

namespace {

constexpr size_t MaxLocals = 8192;
constexpr size_t MaxUpvalues = 256;

bool inFunction(size_t count) { return count > 1; }

}

void Resolver::collectGlobals(Stmt& stmt) {
    switch (stmt.kind) {
        case StmtKind::Expression: {
            Expr& expr = *static_cast<ExpressionStmt&>(stmt).expression;
            if (expr.kind == ExprKind::Assign) {
                AssignExpr& assign = static_cast<AssignExpr&>(expr);
                if (!assign.targetIsCompound && assign.target->kind == ExprKind::Variable) {
                    declaredGlobals.insert(static_cast<VariableExpr&>(*assign.target).name);
                }
            }
            break;
        }
        case StmtKind::Function:
            declaredGlobals.insert(static_cast<FunctionStmt&>(stmt).name);
            break;
        case StmtKind::Class:
            declaredGlobals.insert(static_cast<ClassStmt&>(stmt).name);
            break;
        case StmtKind::Import:
            declaredGlobals.insert(static_cast<ImportStmt&>(stmt).name);
            break;
        default:
            break;
    }
}

void Resolver::resolve(Program& program) {
    functions.emplace_back();
    for (StmtPtr& stmt : program) collectGlobals(*stmt);
    for (StmtPtr& stmt : program) statement(*stmt);
    functions.pop_back();
}

void Resolver::beginScope() {
    FnScope& fn = functions.back();
    fn.scopeStarts.push_back(fn.locals.size());
    fn.scopeCaptured.push_back(false);
}

size_t Resolver::endScope() {
    FnScope& fn = functions.back();
    size_t start = fn.scopeStarts.back();
    fn.scopeStarts.pop_back();
    fn.scopeCaptured.pop_back();
    size_t count = fn.locals.size() - start;
    fn.locals.resize(start);
    return count;
}

bool Resolver::capturedAbove(size_t base) const {
    const FnScope& fn = functions.back();
    for (size_t scope = 0; scope < fn.scopeStarts.size(); ++scope) {
        if (fn.scopeStarts[scope] >= base && fn.scopeCaptured[scope]) return true;
    }
    return false;
}

uint16_t Resolver::declareLocal(const std::string& name, int line, int column) {
    FnScope& fn = functions.back();
    if (fn.locals.size() >= MaxLocals) throw SyntaxError{line, column, "loba teuing variabel lokal"};
    fn.locals.push_back(name);
    if (fn.locals.size() > fn.maxLocals) fn.maxLocals = fn.locals.size();
    return static_cast<uint16_t>(fn.locals.size() - 1);
}

bool Resolver::findLocal(const std::string& name, uint16_t& slot) const {
    const FnScope& fn = functions.back();
    for (size_t index = fn.locals.size(); index > 0; --index) {
        if (fn.locals[index - 1] == name) {
            slot = static_cast<uint16_t>(index - 1);
            return true;
        }
    }
    return false;
}

int Resolver::addUpvalueDesc(size_t fnIndex, bool isLocal, uint16_t target) {
    FnScope& fn = functions[fnIndex];
    for (size_t index = 0; index < fn.upvalues.size(); ++index) {
        const UpvalueDesc& existing = fn.upvalues[index];
        if (existing.isLocal == isLocal && existing.index == target) {
            return static_cast<int>(index);
        }
    }
    if (fn.upvalues.size() >= MaxUpvalues) throw SyntaxError{1, 1, "loba teuing upvalue"};
    UpvalueDesc desc;
    desc.isLocal = isLocal;
    desc.index = target;
    fn.upvalues.push_back(desc);
    return static_cast<int>(fn.upvalues.size() - 1);
}

void Resolver::markCaptured(size_t fnIndex, uint16_t slot) {
    FnScope& fn = functions[fnIndex];
    for (size_t scope = 0; scope < fn.scopeStarts.size(); ++scope) {
        size_t begin = fn.scopeStarts[scope];
        size_t end = scope + 1 < fn.scopeStarts.size() ? fn.scopeStarts[scope + 1] : fn.locals.size();
        if (slot >= begin && slot < end) {
            fn.scopeCaptured[scope] = true;
            return;
        }
    }
}

int Resolver::resolveUpvalueAt(size_t fnIndex, const std::string& name) {
    if (fnIndex == 0) return -1;
    FnScope& parent = functions[fnIndex - 1];
    for (size_t index = parent.locals.size(); index > 0; --index) {
        if (parent.locals[index - 1] == name) {
            markCaptured(fnIndex - 1, static_cast<uint16_t>(index - 1));
            return addUpvalueDesc(fnIndex, true, static_cast<uint16_t>(index - 1));
        }
    }
    if (fnIndex - 1 > 0) {
        int outer = resolveUpvalueAt(fnIndex - 1, name);
        if (outer >= 0) return addUpvalueDesc(fnIndex, false, static_cast<uint16_t>(outer));
    }
    return -1;
}

void Resolver::collectAssigned(Stmt& stmt, std::vector<std::string>& names) {
    switch (stmt.kind) {
        case StmtKind::Expression: {
            Expr& expr = *static_cast<ExpressionStmt&>(stmt).expression;
            if (expr.kind == ExprKind::Assign) {
                AssignExpr& assign = static_cast<AssignExpr&>(expr);
                if (!assign.targetIsCompound && assign.target->kind == ExprKind::Variable) {
                    names.push_back(static_cast<VariableExpr&>(*assign.target).name);
                }
            }
            break;
        }
        case StmtKind::Function:
            names.push_back(static_cast<FunctionStmt&>(stmt).name);
            break;
        case StmtKind::Class:
            names.push_back(static_cast<ClassStmt&>(stmt).name);
            break;
        case StmtKind::Import:
            names.push_back(static_cast<ImportStmt&>(stmt).name);
            break;
        case StmtKind::Block:
            for (StmtPtr& inner : static_cast<BlockStmt&>(stmt).statements) collectAssigned(*inner, names);
            break;
        case StmtKind::If:
            collectAssigned(*static_cast<IfStmt&>(stmt).thenBranch, names);
            if (static_cast<IfStmt&>(stmt).elseBranch) {
                collectAssigned(*static_cast<IfStmt&>(stmt).elseBranch, names);
            }
            break;
        case StmtKind::While:
            collectAssigned(*static_cast<WhileStmt&>(stmt).body, names);
            break;
        case StmtKind::For:
            collectAssigned(*static_cast<ForStmt&>(stmt).body, names);
            break;
        case StmtKind::Try: {
            TryStmt& node = static_cast<TryStmt&>(stmt);
            collectAssigned(*node.body, names);
            collectAssigned(*node.handler, names);
            break;
        }
        default:
            break;
    }
}

void Resolver::function(FunctionStmt& fn, bool isMethod) {
    functions.emplace_back();
    functions.back().isMethod = isMethod;
    if (isMethod) declareLocal("ieu", fn.line, 1);
    else declareLocal("(callee)", fn.line, 1);
    for (const Param& param : fn.params) declareLocal(param.name, param.line, 1);
    if (fn.restIndex >= 0) declareLocal(fn.restName, fn.line, 1);

    std::vector<std::string> assigned;
    collectAssigned(*fn.body, assigned);
    for (const std::string& name : assigned) {
        uint16_t slot = 0;
        if (findLocal(name, slot)) continue;
        if (resolveUpvalueAt(functions.size() - 1, name) >= 0) continue;
        if (declaredGlobals.count(name) > 0) continue;
        declareLocal(name, fn.line, 1);
    }


    for (const Param& param : fn.params) {
        if (param.defaultValue) expression(*param.defaultValue, false);
    }

    beginScope();
    for (StmtPtr& inner : static_cast<BlockStmt&>(*fn.body).statements) statement(*inner);
    static_cast<BlockStmt&>(*fn.body).localCount = endScope();

    FnScope& scope = functions.back();
    if (scope.maxLocals > 65535) throw SyntaxError{fn.line, 1, "loba teuing variabel lokal"};
    fn.localCount = static_cast<uint16_t>(scope.maxLocals);
    if (scope.upvalues.size() > 255) throw SyntaxError{fn.line, 1, "loba teuing upvalue"};
    fn.upvalueCount = static_cast<uint16_t>(scope.upvalues.size());
    fn.upvalueDescs = scope.upvalues;
    functions.pop_back();
}

void Resolver::declareStoreName(const std::string& name, Stmt& stmt) {
    auto markLocal = [&](uint16_t slot) {
        if (stmt.kind == StmtKind::Function) {
            static_cast<FunctionStmt&>(stmt).scope = VarScope::Local;
            static_cast<FunctionStmt&>(stmt).slot = slot;
        } else if (stmt.kind == StmtKind::Class) {
            static_cast<ClassStmt&>(stmt).scope = VarScope::Local;
            static_cast<ClassStmt&>(stmt).slot = slot;
        }
    };
    if (!inFunction(functions.size())) {
        if (functions.back().scopeStarts.empty() || declaredGlobals.count(name) > 0) {
            declaredGlobals.insert(name);
            return;
        }
        uint16_t slot = 0;
        if (findLocal(name, slot)) {
            markLocal(slot);
            return;
        }
        declareLocal(name, stmt.line, 1);
        if (stmt.kind == StmtKind::Function) static_cast<FunctionStmt&>(stmt).declares = true;
        else if (stmt.kind == StmtKind::Class) static_cast<ClassStmt&>(stmt).declares = true;
        return;
    }
    uint16_t slot = 0;
    if (findLocal(name, slot)) {
        markLocal(slot);
        return;
    }
    int upvalue = resolveUpvalueAt(functions.size() - 1, name);
    if (upvalue >= 0) {
        if (stmt.kind == StmtKind::Function) {
            static_cast<FunctionStmt&>(stmt).scope = VarScope::Upvalue;
            static_cast<FunctionStmt&>(stmt).slot = static_cast<uint16_t>(upvalue);
        } else if (stmt.kind == StmtKind::Class) {
            static_cast<ClassStmt&>(stmt).scope = VarScope::Upvalue;
            static_cast<ClassStmt&>(stmt).slot = static_cast<uint16_t>(upvalue);
        }
        return;
    }
    slot = declareLocal(name, stmt.line, 1);
    markLocal(slot);
}

void Resolver::statement(Stmt& stmt) {
    switch (stmt.kind) {
        case StmtKind::Expression:
            expression(*static_cast<ExpressionStmt&>(stmt).expression, true);
            break;
        case StmtKind::Block: {
            BlockStmt& node = static_cast<BlockStmt&>(stmt);
            beginScope();
            for (StmtPtr& inner : node.statements) statement(*inner);
            bool captured = functions.back().scopeCaptured.back();
            node.localCount = endScope();
            node.closeUpvalues = captured && node.localCount > 0;
            break;
        }
        case StmtKind::If: {
            IfStmt& node = static_cast<IfStmt&>(stmt);
            expression(*node.condition, false);
            statement(*node.thenBranch);
            if (node.elseBranch) statement(*node.elseBranch);
            break;
        }
        case StmtKind::While: {
            WhileStmt& node = static_cast<WhileStmt&>(stmt);
            expression(*node.condition, false);
            size_t base = functions.back().locals.size();
            functions.back().loops.push_back(LoopInfo{base, base});
            statement(*node.body);
            functions.back().loops.pop_back();
            node.breakCaptured = capturedAbove(base);
            node.continueCaptured = node.breakCaptured;
            break;
        }
        case StmtKind::For: {
            ForStmt& node = static_cast<ForStmt&>(stmt);
            expression(*node.iterable, false);
            beginScope();
            node.hiddenSlot = declareLocal("(iterable)", stmt.line, 1);
            declareLocal("(kursor)", stmt.line, 1);
            beginScope();
            uint16_t varSlot = declareLocal(node.variable, stmt.line, 1);
            functions.back().loops.push_back(
                LoopInfo{static_cast<size_t>(node.hiddenSlot) + 2,
                         static_cast<size_t>(node.hiddenSlot) + 3});
            statement(*node.body);
            node.varCaptured = functions.back().scopeCaptured.back();
            functions.back().loops.pop_back();
            node.breakCaptured = capturedAbove(static_cast<size_t>(node.hiddenSlot) + 2);
            node.continueCaptured = capturedAbove(static_cast<size_t>(node.hiddenSlot) + 3);
            (void)varSlot;
            endScope();
            endScope();
            break;
        }
        case StmtKind::Break: {
            if (functions.back().loops.empty()) {
                throw SyntaxError{stmt.line, 1, "eureun kudu di jero salila atawa pikeun"};
            }
            LoopInfo& loop = functions.back().loops.back();
            BreakStmt& node = static_cast<BreakStmt&>(stmt);
            node.popCount = inFunction(functions.size()) ? 0
                                                  : functions.back().locals.size() - loop.breakBase;
            node.closeUpvalues = capturedAbove(loop.breakBase);
            break;
        }
        case StmtKind::Continue: {
            if (functions.back().loops.empty()) {
                throw SyntaxError{stmt.line, 1, "teruskeun kudu di jero salila atawa pikeun"};
            }
            LoopInfo& loop = functions.back().loops.back();
            ContinueStmt& node = static_cast<ContinueStmt&>(stmt);
            node.popCount = inFunction(functions.size())
                                ? 0
                                : functions.back().locals.size() - loop.continueBase;
            node.closeUpvalues = capturedAbove(loop.continueBase);
            break;
        }
        case StmtKind::Function: {
            FunctionStmt& node = static_cast<FunctionStmt&>(stmt);
            function(node, false);
            declareStoreName(node.name, node);
            break;
        }
        case StmtKind::Class: {
            ClassStmt& node = static_cast<ClassStmt&>(stmt);
            if (node.hasSuperclass) {
                uint16_t slot = 0;
                if (findLocal(node.superclass, slot)) {
                    node.superScope = VarScope::Local;
                    node.superSlot = slot;
                } else {
                    int upvalue = resolveUpvalueAt(functions.size() - 1, node.superclass);
                    if (upvalue >= 0) {
                        node.superScope = VarScope::Upvalue;
                        node.superSlot = static_cast<uint16_t>(upvalue);
                    }
                }
            }
            ++classDepth;
            for (StmtPtr& method : node.methods) {
                function(*static_cast<FunctionStmt*>(method.get()), true);
            }
            --classDepth;
            declareStoreName(node.name, node);
            break;
        }
        case StmtKind::Return: {
            if (!inFunction(functions.size())) {
                throw SyntaxError{stmt.line, 1, "balikeun ngan sah di jero pagawean"};
            }
            ReturnStmt& node = static_cast<ReturnStmt&>(stmt);
            if (node.value) expression(*node.value, false);
            break;
        }
        case StmtKind::Import: {
            ImportStmt& node = static_cast<ImportStmt&>(stmt);
            if (inFunction(functions.size())) {
                uint16_t slot = 0;
                if (!findLocal(node.name, slot)) {
                    slot = declareLocal(node.name, stmt.line, 1);
                }
                node.scope = VarScope::Local;
                node.slot = slot;
            }
            break;
        }
        case StmtKind::Try: {
            TryStmt& node = static_cast<TryStmt&>(stmt);
            beginScope();
            for (StmtPtr& inner : static_cast<BlockStmt&>(*node.body).statements) statement(*inner);
            bool bodyCaptured = functions.back().scopeCaptured.back();
            static_cast<BlockStmt&>(*node.body).localCount = endScope();
            static_cast<BlockStmt&>(*node.body).closeUpvalues =
                bodyCaptured && static_cast<BlockStmt&>(*node.body).localCount > 0;
            beginScope();
            if (node.hasCatchVar) node.catchSlot = declareLocal(node.catchVar, stmt.line, 1);
            beginScope();
            for (StmtPtr& inner : static_cast<BlockStmt&>(*node.handler).statements) statement(*inner);
            bool handlerCaptured = functions.back().scopeCaptured.back();
            static_cast<BlockStmt&>(*node.handler).localCount = endScope();
            static_cast<BlockStmt&>(*node.handler).closeUpvalues =
                handlerCaptured && static_cast<BlockStmt&>(*node.handler).localCount > 0;
            node.closeUpvalues = functions.back().scopeCaptured.back();
            endScope();
            break;
        }
        case StmtKind::Throw:
            expression(*static_cast<ThrowStmt&>(stmt).value, false);
            break;
    }
}

void Resolver::resolveTarget(Expr& target, bool atStatementLevel) {
    switch (target.kind) {
        case ExprKind::Variable:
            expression(target, atStatementLevel);
            break;
        case ExprKind::Property: {
            PropertyExpr& node = static_cast<PropertyExpr&>(target);
            expression(*node.object, false);
            break;
        }
        case ExprKind::Index: {
            IndexExpr& node = static_cast<IndexExpr&>(target);
            expression(*node.object, false);
            expression(*node.index, false);
            break;
        }
        default:
            throw SyntaxError{target.line, target.column, "target tugas teu sah"};
    }
}

void Resolver::expression(Expr& expr, bool atStatementLevel) {
    switch (expr.kind) {
        case ExprKind::Number:
        case ExprKind::String:
        case ExprKind::Literal:
            break;
        case ExprKind::Variable: {
            VariableExpr& node = static_cast<VariableExpr&>(expr);
            if (node.name == "ieu" && !functions.back().isMethod) {
                throw SyntaxError{expr.line, expr.column, "ieu ngan sah di jero metoda"};
            }
            uint16_t slot = 0;
            if (findLocal(node.name, slot)) {
                node.scope = VarScope::Local;
                node.slot = slot;
            } else {
                int upvalue = resolveUpvalueAt(functions.size() - 1, node.name);
                if (upvalue >= 0) {
                    node.scope = VarScope::Upvalue;
                    node.slot = static_cast<uint16_t>(upvalue);
                }
            }
            break;
        }
        case ExprKind::Unary:
            expression(*static_cast<UnaryExpr&>(expr).operand, false);
            break;
        case ExprKind::Binary: {
            BinaryExpr& node = static_cast<BinaryExpr&>(expr);
            expression(*node.left, false);
            expression(*node.right, false);
            break;
        }
        case ExprKind::Logical: {
            LogicalExpr& node = static_cast<LogicalExpr&>(expr);
            expression(*node.left, false);
            expression(*node.right, false);
            break;
        }
        case ExprKind::Assign: {
            AssignExpr& node = static_cast<AssignExpr&>(expr);
            expression(*node.value, false);
            resolveTarget(*node.target, atStatementLevel);
            if (node.target->kind == ExprKind::Variable) {
                const VariableExpr& target = static_cast<const VariableExpr&>(*node.target);
                if (target.scope == VarScope::Local) {
                    node.scope = VarScope::Local;
                    node.slot = target.slot;
                } else if (target.scope == VarScope::Upvalue) {
                    node.scope = VarScope::Upvalue;
                    node.slot = target.slot;
                } else if (declaredGlobals.count(target.name) > 0) {
                    node.scope = VarScope::Global;
                } else if (!inFunction(functions.size()) &&
                           functions.back().scopeStarts.empty()) {
                    node.scope = VarScope::Global;
                    declaredGlobals.insert(target.name);
                } else if (atStatementLevel && !node.targetIsCompound &&
                           !inFunction(functions.size())) {
                    node.scope = VarScope::Local;
                    node.declares = true;
                    node.slot = declareLocal(target.name, expr.line, expr.column);
                } else {
                    throw SyntaxError{expr.line, expr.column,
                                      "teu kapanggih variabel \"" + target.name + "\""};
                }
            }
            break;
        }
        case ExprKind::Call: {
            CallExpr& node = static_cast<CallExpr&>(expr);
            expression(*node.callee, false);
            for (ExprPtr& argument : node.arguments) expression(*argument, false);
            break;
        }
        case ExprKind::Property: {
            PropertyExpr& node = static_cast<PropertyExpr&>(expr);
            expression(*node.object, false);
            break;
        }
        case ExprKind::Index: {
            IndexExpr& node = static_cast<IndexExpr&>(expr);
            expression(*node.object, false);
            expression(*node.index, false);
            break;
        }
        case ExprKind::Slice: {
            SliceExpr& node = static_cast<SliceExpr&>(expr);
            expression(*node.object, false);
            if (node.hasStart) expression(*node.start, false);
            if (node.hasEnd) expression(*node.end, false);
            break;
        }
        case ExprKind::Super:
            if (classDepth == 0) {
                throw SyntaxError{expr.line, expr.column, "induk ngan sah di jero metoda"};
            }
            break;
        case ExprKind::List:
            for (ExprPtr& element : static_cast<ListExpr&>(expr).elements) {
                expression(*element, false);
            }
            break;
        case ExprKind::Map: {
            MapExpr& node = static_cast<MapExpr&>(expr);
            for (MapEntry& entry : node.entries) {
                expression(*entry.key, false);
                expression(*entry.value, false);
            }
            break;
        }
        case ExprKind::Set:
            for (ExprPtr& element : static_cast<SetExpr&>(expr).elements) {
                expression(*element, false);
            }
            break;
        case ExprKind::Tuple:
            for (ExprPtr& element : static_cast<TupleExpr&>(expr).elements) {
                expression(*element, false);
            }
            break;
    }
}

}
