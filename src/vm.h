#pragma once

#include <cstddef>
#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "chunk.h"
#include "memory.h"
#include "native.h"
#include "object.h"
#include "table.h"

namespace sunda {

enum class InterpretResult {
    Ok,
    CompileError,
    RuntimeError,
};

struct CatchPoint {
    const uint8_t* ip = nullptr;
    Value* sp = nullptr;
};

struct CallFrame {
    ObjClosure* closure = nullptr;
    const uint8_t* ip = nullptr;
    Value* slots = nullptr;
    ObjModule* module = nullptr;
    std::vector<CatchPoint> catches;
    bool isInit = false;
    bool isModule = false;
};

struct VM {
    static constexpr size_t StackMax = 1 << 16;
    static constexpr size_t FramesMax = 1024;

    Heap heap;
    Table modules;
    Table builtins;
    Table methodTables[5];
    std::vector<std::string> scriptArgs;
    std::deque<std::string> sources;
    std::unique_ptr<Value[]> stack;
    Value* stackTop = nullptr;
    std::vector<CallFrame> frames;
    std::string pendingError;
    std::vector<std::string> loadingModules;
    bool replMode = false;

    VM();
    InterpretResult interpret(std::string_view text, const std::string& path = "");
    bool compileCheck(std::string_view text);
    void raise(std::string message);

private:
    InterpretResult run();
    bool loadModule(const std::string& name, Value& out, bool& started);
    bool callCallable(Value callee, int argc);
    bool callFunctionCommon(ObjClosure* closure, int argc, bool method);
    void invoke(Value receiver, ObjString* name, int argc);
    ObjUpvalue* captureUpvalue(Value* local);
    void closeUpvalues(Value* last);
    int iterate(Value* iterableSlot, Value& element);
    void reportError(const uint8_t* ip, ObjClosure* closure, const Value& errorValue);
    bool stackHasRoom(size_t extra) {
        return static_cast<size_t>(stackTop - stack.get()) + extra <= StackMax;
    }
};

}
