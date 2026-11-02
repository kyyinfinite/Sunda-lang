#pragma once

#include <string>
#include <unordered_set>
#include <vector>

#include "ast.h"

namespace sunda {

class Resolver {
public:
    void resolve(Program& program);

private:
    struct LoopInfo {
        size_t breakBase = 0;
        size_t continueBase = 0;
        bool captured = false;
    };

    struct FnScope {
        std::vector<std::string> locals;
        std::vector<size_t> scopeStarts;
        std::vector<bool> scopeCaptured;
        std::vector<UpvalueDesc> upvalues;
        std::vector<LoopInfo> loops;
        size_t maxLocals = 0;
        bool isMethod = false;
    };

    void statement(Stmt& stmt);
    void expression(Expr& expr, bool atStatementLevel);
    void function(FunctionStmt& fn, bool isMethod);
    void collectAssigned(Stmt& stmt, std::vector<std::string>& names);
    void collectGlobals(Stmt& stmt);
    void resolveTarget(Expr& target, bool atStatementLevel);
    void declareStoreName(const std::string& name, Stmt& stmt);

    void beginScope();
    size_t endScope();
    bool capturedAbove(size_t base) const;
    uint16_t declareLocal(const std::string& name, int line, int column);
    bool findLocal(const std::string& name, uint16_t& slot) const;
    int resolveUpvalueAt(size_t fnIndex, const std::string& name);
    int addUpvalueDesc(size_t fnIndex, bool isLocal, uint16_t target);
    void markCaptured(size_t fnIndex, uint16_t slot);

    std::vector<FnScope> functions;
    std::unordered_set<std::string> declaredGlobals;
    int classDepth = 0;
};

}
