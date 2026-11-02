#include "vm.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "compiler.h"
#include "lexer.h"
#include "modules.h"
#include "parser.h"
#include "resolver.h"

namespace sunda {

#if defined(__GNUC__) || defined(__clang__)
#define SUNDA_USE_COMPUTED_GOTO 1
#endif

namespace {

void printStdout(const std::string& text) {
    std::fwrite(text.data(), 1, text.size(), stdout);
}

ObjString* concatenate(Heap& heap, Value left, Value right) {
    std::string joined = asString(left)->chars;
    joined += asString(right)->chars;
    return takeString(heap, std::move(joined));
}

void echoValue(Value value) {
    std::string text = valueToString(value);
    text += '\n';
    printStdout(text);
}

}

VM::VM() : stack(std::make_unique<Value[]>(StackMax)) {
    stackTop = stack.get();
    frames.reserve(64);
    heap.vm = this;
    heap.pauseGC();
    registerBuiltins(*this);
    heap.resumeGC();
    const char* stress = std::getenv("SUNDA_GC_STRESS");
    heap.stressGC = stress != nullptr && *stress != '0';
}

bool VM::compileCheck(std::string_view text) {
    heap.pauseGC();
    bool ok = true;
    try {
        Lexer lexer(text);
        Parser parser(lexer.tokenize());
        Program program = parser.parse();
        Resolver resolver;
        resolver.resolve(program);
        Compiler compiler(heap);
        compiler.compile(program, text, "<utama>");
    } catch (const SyntaxError& error) {
        std::string report = formatError(text, error.line, error.column, error.message);
        std::fputs(report.c_str(), stderr);
        ok = false;
    }
    heap.resumeGC();
    return ok;
}

void VM::raise(std::string message) {
    if (pendingError.empty()) pendingError = std::move(message);
}

bool VM::callFunctionCommon(ObjClosure* closure, int argc, bool method) {
    ObjFunction* fn = closure->function;
    if (frames.size() >= FramesMax) {
        raise("kalobaan tumpukan panggilan (rekursi kadalam teuing)");
        return false;
    }
    if (!stackHasRoom(static_cast<size_t>(fn->slotCount) + 80)) {
        raise("kalobaan tumpukan (rekursi kadalam teuing)");
        return false;
    }
    int total = argc + (method ? 1 : 0);
    bool fits = total >= fn->arityMin && (fn->arityMax < 0 || total <= fn->arityMax);
    if (!fits) {
        std::string expected = fn->arityMax < 0
                                   ? std::to_string(fn->arityMin) + " atawa leuwih"
                                   : fn->arityMin == fn->arityMax
                                       ? std::to_string(fn->arityMin)
                                       : std::to_string(fn->arityMin) + "-" +
                                             std::to_string(fn->arityMax);
        raise("argumen teu cocog pikeun \"" + fn->name->chars + "\" (butuh " + expected +
              ", dikirim " + std::to_string(total) + ")");
        return false;
    }
    int userParams = fn->paramSlots - (fn->isMethod ? 1 : 0);
    Value* slots = stackTop - argc - 1;
    if (fn->restSlot >= 0) {
        ObjList* rest = newList(heap);
        for (int index = fn->restSlot; index <= argc; ++index) {
            rest->items.push_back(slots[index]);
        }
        slots[fn->restSlot] = objValue(rest);
    }
    if (fn->firstDefaultSlot >= 0) {
        for (int slot = argc + 1; slot <= userParams; ++slot) slots[slot] = NilValue;
    }
    Value* newTop = slots + fn->slotCount;
    for (Value* slot = stackTop; slot < newTop; ++slot) *slot = NilValue;
    stackTop = newTop;
    frames.emplace_back();
    CallFrame& frame = frames.back();
    frame.closure = closure;
    frame.ip = fn->chunk.code.data();
    frame.slots = slots;
    frame.module = frames[frames.size() - 2].module;
    return true;
}

bool VM::callCallable(Value callee, int argc) {
    if (isNative(callee)) {
        ObjNative* native = asNative(callee);
        if (native->arity >= 0 && native->arity != argc) {
            raise("argumen teu cocog pikeun \"" + native->name->chars + "\" (butuh " +
                  std::to_string(native->arity) + ", dikirim " + std::to_string(argc) + ")");
            return false;
        }
        Value* args = stackTop - argc;
        Value receiver = stackTop[-argc - 1];
        Value result = native->function(*this, receiver, argc, args);
        stackTop -= argc + 1;
        if (!pendingError.empty()) return false;
        stackTop[0] = result;
        stackTop += 1;
        return true;
    }
    if (isClosure(callee)) return callFunctionCommon(asClosure(callee), argc, false);
    if (isBoundMethod(callee)) {
        ObjBoundMethod* bound = asBoundMethod(callee);
        stackTop[-argc - 1] = bound->receiver;
        if (bound->method->type == ObjType::Native) {
            ObjNative* native = static_cast<ObjNative*>(bound->method);
            if (native->arity >= 0 && native->arity != argc) {
                raise("argumen teu cocog pikeun \"" + native->name->chars + "\" (butuh " +
                      std::to_string(native->arity) + ", dikirim " + std::to_string(argc) + ")");
                return false;
            }
            Value* args = stackTop - argc;
            Value result = native->function(*this, bound->receiver, argc, args);
            stackTop -= argc + 1;
            if (!pendingError.empty()) return false;
            stackTop[0] = result;
            stackTop += 1;
            return true;
        }
        return callFunctionCommon(static_cast<ObjClosure*>(bound->method), argc, true);
    }
    if (isClass(callee)) {
        ObjClass* klass = asClass(callee);
        Value initValue;
        bool hasInit = klass->methods.get(objValue(copyString(heap, "awal")), initValue);
        if (!hasInit) {
            if (argc != 0) {
                raise("argumen teu cocog pikeun \"" + klass->name->chars + "\" (butuh 0, dikirim " +
                      std::to_string(argc) + ")");
                return false;
            }
            ObjInstance* instance = newInstance(heap, klass);
            stackTop[-1] = objValue(instance);
            return true;
        }
        ObjInstance* instance = newInstance(heap, klass);
        stackTop[-argc - 1] = objValue(instance);
        if (callFunctionCommon(asClosure(initValue), argc, true)) {
            frames.back().isInit = true;
            return true;
        }
        return false;
    }
    raise("teu bisa dipanggil nilai jenis " + std::string(typeName(callee)));
    return false;
}

void VM::invoke(Value receiver, ObjString* name, int argc) {
    Value key = objValue(name);
    if (isInstance(receiver)) {
        ObjInstance* instance = asInstance(receiver);
        ObjClass* klass = instance->klass;
        Value method;
        ObjClass* owner = nullptr;
        while (klass != nullptr) {
            if (klass->methods.get(key, method)) {
                owner = klass;
                break;
            }
            klass = klass->superclass;
        }
        if (owner != nullptr && isClosure(method)) {
            callFunctionCommon(asClosure(method), argc, true);
            return;
        }
        Value field;
        if (instance->fields.get(key, field) && isCallable(field)) {
            stackTop[-argc - 1] = field;
            callCallable(field, argc);
            return;
        }
        raise("teu boga metoda \"" + name->chars + "\" dina " +
              std::string(instance->klass->name->chars));
        return;
    }
    if (isModule(receiver)) {
        ObjModule* module = asModule(receiver);
        Value member;
        if (!module->globals.get(key, member)) {
            raise("modul \"" + module->name->chars + "\" teu boga \"" + name->chars + "\"");
            return;
        }
        stackTop[-argc - 1] = member;
        callCallable(member, argc);
        return;
    }
    int tableIndex = -1;
    if (isList(receiver)) tableIndex = 0;
    else if (isMap(receiver)) tableIndex = 1;
    else if (isSet(receiver)) tableIndex = 2;
    else if (isString(receiver)) tableIndex = 3;
    else if (isTuple(receiver)) tableIndex = 4;
    if (tableIndex >= 0) {
        Value method;
        if (methodTables[tableIndex].get(key, method)) {
            ObjNative* native = asNative(method);
            if (native->arity >= 0 && native->arity != argc) {
                raise("argumen teu cocog pikeun \"" + name->chars + "\" (butuh " +
                      std::to_string(native->arity) + ", dikirim " + std::to_string(argc) + ")");
                return;
            }
            Value* args = stackTop - argc;
            Value result = native->function(*this, receiver, argc, args);
            stackTop -= argc + 1;
            if (!pendingError.empty()) return;
            stackTop[0] = result;
            stackTop += 1;
            return;
        }
        raise("teu boga metoda \"" + name->chars + "\" pikeun " + typeName(receiver));
        return;
    }
    raise("teu bisa ngala metoda \"" + name->chars + "\" tina " + typeName(receiver));
}

ObjUpvalue* VM::captureUpvalue(Value* local) {
    ObjUpvalue* previous = nullptr;
    ObjUpvalue* upvalue = heap.openUpvalues;
    while (upvalue != nullptr && upvalue->location > local) {
        previous = upvalue;
        upvalue = upvalue->openNext;
    }
    if (upvalue != nullptr && upvalue->location == local) return upvalue;
    ObjUpvalue* created = allocateObject<ObjUpvalue>(heap, ObjType::Upvalue);
    created->location = local;
    created->openNext = upvalue;
    if (previous == nullptr) heap.openUpvalues = created;
    else previous->openNext = created;
    return created;
}

void VM::closeUpvalues(Value* last) {
    while (heap.openUpvalues != nullptr && heap.openUpvalues->location >= last) {
        ObjUpvalue* upvalue = heap.openUpvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = nullptr;
        heap.openUpvalues = upvalue->openNext;
    }
}

int VM::iterate(Value* iterableSlot, Value& element) {
    Value iterable = iterableSlot[0];
    double cursor = asNumber(iterableSlot[1]);
    if (isRange(iterable)) {
        ObjRange* range = asRange(iterable);
        double current = range->start + cursor * range->step;
        bool alive = range->step > 0 ? current < range->stop : current > range->stop;
        if (!alive) return 0;
        element = numberValue(current);
        iterableSlot[1] = numberValue(cursor + 1);
        return 1;
    }
    if (isList(iterable)) {
        const std::vector<Value>& items = asList(iterable)->items;
        if (static_cast<size_t>(cursor) >= items.size()) return 0;
        element = items[static_cast<size_t>(cursor)];
        iterableSlot[1] = numberValue(cursor + 1);
        return 1;
    }
    if (isTuple(iterable)) {
        const std::vector<Value>& items = asTuple(iterable)->items;
        if (static_cast<size_t>(cursor) >= items.size()) return 0;
        element = items[static_cast<size_t>(cursor)];
        iterableSlot[1] = numberValue(cursor + 1);
        return 1;
    }
    if (isString(iterable)) {
        const std::string& chars = asString(iterable)->chars;
        size_t position = static_cast<size_t>(cursor);
        if (position >= chars.size()) return 0;
        unsigned char lead = static_cast<unsigned char>(chars[position]);
        size_t length = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
        if (position + length > chars.size()) length = chars.size() - position;
        element = objValue(copyString(heap, std::string_view(chars).substr(position, length)));
        iterableSlot[1] = numberValue(cursor + length);
        return 1;
    }
    if (isMap(iterable) || isSet(iterable)) {
        Table& table = isMap(iterable) ? asMap(iterable)->table : asSet(iterable)->table;
        const std::vector<Entry>& entries = table.raw();
        for (size_t index = static_cast<size_t>(cursor); index < entries.size(); ++index) {
            const Entry& entry = entries[index];
            if (entry.key.bits == EmptyKeyValue.bits || entry.key.bits == TombstoneValue.bits) continue;
            element = entry.key;
            iterableSlot[1] = numberValue(static_cast<double>(index + 1));
            return 1;
        }
        return 0;
    }
    return -1;
}

void VM::reportError(const uint8_t* ip, ObjClosure* closure, const Value& errorValue) {
    const Chunk& chunk = closure->function->chunk;
    size_t index = static_cast<size_t>(ip - chunk.code.data());
    int line = chunk.lines[index > 0 ? index - 1 : 0];
    std::string_view source = closure->function->source;
    std::string_view text = sourceLine(source, line);
    size_t indent = text.find_first_not_of(" \t");
    if (indent == std::string_view::npos) indent = 0;
    std::string message =
        isString(errorValue) ? asString(errorValue)->chars : valueToString(errorValue);
    std::string report = formatError(source, line, static_cast<int>(indent) + 1, message);
    report += "  [baris " + std::to_string(line) + "] di " + closure->function->name->chars + "\n";
    std::fputs(report.c_str(), stderr);
}

bool VM::loadModule(const std::string& name, Value& out, bool& started) {
    started = false;
    heap.pauseGC();
    ObjString* key = copyString(heap, name);
    Value cached;
    if (modules.get(objValue(key), cached)) {
        out = cached;
        heap.resumeGC();
        return true;
    }
    if (nativeModule(*this, name, out)) {
        modules.set(objValue(key), out);
        heap.resumeGC();
        return true;
    }
    heap.resumeGC();
    std::string dir = frames.empty() ? std::string(".") : frames.back().module->dir;
    std::string path = dir + "/" + name + ".sd";
    for (const std::string& pending : loadingModules) {
        if (pending == path) {
            raise("impor sirkular dideteksi: \"" + name + "\"");
            return false;
        }
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        raise("modul teu kapanggih: \"" + name + "\"");
        return false;
    }
    std::stringstream buffer;
    buffer << input.rdbuf();
    sources.push_back(buffer.str());
    std::string_view sourceView = sources.back();

    heap.pauseGC();
    ObjFunction* function = nullptr;
    try {
        Lexer lexer(sourceView);
        Parser parser(lexer.tokenize());
        Program program = parser.parse();
        Resolver resolver;
        resolver.resolve(program);
        Compiler compiler(heap);
        function = compiler.compile(program, sourceView, "<modul " + name + ">");
    } catch (const SyntaxError& error) {
        heap.resumeGC();
        std::string report = formatError(sourceView, error.line, error.column, error.message);
        std::fputs(report.c_str(), stderr);
        raise("gagal nyusun modul \"" + name + "\"");
        return false;
    }
    heap.resumeGC();

    heap.pauseGC();
    ObjString* moduleName = copyString(heap, name);
    ObjModule* module = newModule(heap, moduleName);
    module->dir = dir;
    modules.set(objValue(moduleName), objValue(module));
    loadingModules.push_back(path);

    ObjClosure* closure = newClosure(heap, function);
    frames.emplace_back();
    CallFrame& frame = frames.back();
    frame.closure = closure;
    frame.ip = function->chunk.code.data();
    frame.slots = stackTop;
    frame.module = module;
    frame.isModule = true;
    out = objValue(module);
    started = true;
    heap.resumeGC();
    return true;
}

InterpretResult VM::interpret(std::string_view text, const std::string& path) {
    sources.push_back(std::string(text));
    std::string_view sourceView = sources.back();

    heap.pauseGC();
    ObjModule* mainModule = nullptr;
    ObjString* mainName = copyString(heap, "<utama>");
    Value existing;
    if (modules.get(objValue(mainName), existing)) {
        mainModule = asModule(existing);
    } else {
        mainModule = newModule(heap, mainName);
        modules.set(objValue(mainName), objValue(mainModule));
    }
    mainModule->dir = ".";
    if (!path.empty()) {
        size_t slash = path.find_last_of("/\\");
        mainModule->dir = slash == std::string::npos ? "." : path.substr(0, slash);
    }
    heap.resumeGC();

    heap.pauseGC();
    ObjFunction* function = nullptr;
    try {
        Lexer lexer(sourceView);
        Parser parser(lexer.tokenize());
        Program program = parser.parse();
        Resolver resolver;
        resolver.resolve(program);
        Compiler compiler(heap, replMode);
        function = compiler.compile(program, sourceView, "<utama>");
    } catch (const SyntaxError& error) {
        heap.resumeGC();
        std::string report = formatError(sourceView, error.line, error.column, error.message);
        std::fputs(report.c_str(), stderr);
        return InterpretResult::CompileError;
    }

    ObjClosure* closure = newClosure(heap, function);
    frames.emplace_back();
    CallFrame& frame = frames.back();
    frame.closure = closure;
    frame.ip = function->chunk.code.data();
    frame.slots = stackTop;
    frame.module = mainModule;
    heap.resumeGC();

    InterpretResult result = run();
    stackTop = stack.get();
    frames.clear();
    heap.openUpvalues = nullptr;
    return result;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#if defined(__clang__)
#pragma GCC diagnostic ignored "-Wgnu-label-as-value"
#endif

InterpretResult VM::run() {
    const uint8_t* ip = frames.back().ip;
    Value* sp = stackTop;
    Value* slots0 = frames.back().slots;
    const Value* constants = frames.back().closure->function->chunk.constants.data();

#define PUSH(value) (*sp++ = (value))
#define POP() (*--sp)
#define PEEK(distance) (sp[-1 - (distance)])
#define READ_BYTE() (*ip++)
#define READ_SHORT() (ip += 2, static_cast<uint16_t>((ip[-2] << 8) | ip[-1]))
#define READ_NAME() (asString(constants[READ_SHORT()]))
#define SYNC() (stackTop = sp)
#define RUNTIME_ERROR(message)      \
    do {                            \
        raise(message);             \
        goto runtime_error;         \
    } while (false)
#define USER_THROW(value)      \
    do {                       \
        thrownValue = (value); \
        goto user_throw;       \
    } while (false)
#define BINARY_NUMBER(expression, message)                            \
    do {                                                              \
        if (!isNumber(PEEK(0)) || !isNumber(PEEK(1))) RUNTIME_ERROR(message); \
        double b = asNumber(POP());                                   \
        double a = asNumber(POP());                                   \
        PUSH(numberValue(expression));                                \
    } while (false)
#define BINARY_COMPARE(expression)                                    \
    do {                                                              \
        if (!isNumber(PEEK(0)) || !isNumber(PEEK(1))) RUNTIME_ERROR("teu bisa dibandingkeun"); \
        double b = asNumber(POP());                                   \
        double a = asNumber(POP());                                   \
        PUSH(boolValue(expression));                                  \
    } while (false)
#define REFRESH_FRAME()                     \
    do {                                    \
        slots0 = frames.back().slots;       \
        constants = frames.back().closure->function->chunk.constants.data(); \
    } while (false)

#ifdef SUNDA_USE_COMPUTED_GOTO
    static const void* const dispatchTable[] = {
#define X(name) &&L_##name,
        SUNDA_OPCODES(X)
#undef X
    };
#define CASE(name) L_##name
#define DISPATCH() goto *dispatchTable[READ_BYTE()]
#define LOOP_BEGIN DISPATCH();
#define LOOP_END
#else
#define CASE(name) case OpCode::name
#define DISPATCH() continue
#define LOOP_BEGIN \
    for (;;) {     \
        switch (static_cast<OpCode>(READ_BYTE())) {
#define LOOP_END \
        }        \
    }
#endif

    Value thrownValue = NilValue;
    const uint8_t* raiseIp = ip;
    ObjClosure* raiseClosure = frames.back().closure;

    LOOP_BEGIN

    CASE(Constant): {
        PUSH(constants[READ_SHORT()]);
        DISPATCH();
    }
    CASE(Nil): {
        PUSH(NilValue);
        DISPATCH();
    }
    CASE(True): {
        PUSH(TrueValue);
        DISPATCH();
    }
    CASE(False): {
        PUSH(FalseValue);
        DISPATCH();
    }
    CASE(Pop): {
        --sp;
        DISPATCH();
    }
    CASE(PopN): {
        sp -= READ_BYTE();
        DISPATCH();
    }
    CASE(Dup): {
        sp[0] = sp[-1];
        ++sp;
        DISPATCH();
    }
    CASE(Dup2): {
        sp[0] = sp[-2];
        sp[1] = sp[-1];
        sp += 2;
        DISPATCH();
    }
    CASE(GetLocal): {
        PUSH(slots0[READ_SHORT()]);
        DISPATCH();
    }
    CASE(SetLocal): {
        slots0[READ_SHORT()] = PEEK(0);
        DISPATCH();
    }
    CASE(GetGlobal): {
        ObjString* name = READ_NAME();
        Value value;
        if (!frames.back().module->globals.get(objValue(name), value) &&
            !builtins.get(objValue(name), value)) {
            RUNTIME_ERROR("teu kapanggih variabel \"" + name->chars + "\"");
        }
        PUSH(value);
        DISPATCH();
    }
    CASE(SetGlobal): {
        ObjString* name = READ_NAME();
        frames.back().module->globals.set(objValue(name), PEEK(0));
        DISPATCH();
    }
    CASE(GetUpvalue): {
        uint16_t slot = READ_SHORT();
        ObjUpvalue* upvalue = frames.back().closure->upvalues[slot];
        PUSH(upvalue->location != nullptr ? *upvalue->location : upvalue->closed);
        DISPATCH();
    }
    CASE(SetUpvalue): {
        uint16_t slot = READ_SHORT();
        ObjUpvalue* upvalue = frames.back().closure->upvalues[slot];
        if (upvalue->location != nullptr) *upvalue->location = PEEK(0);
        else upvalue->closed = PEEK(0);
        DISPATCH();
    }
    CASE(CloseUpvalueN): {
        closeUpvalues(sp - READ_BYTE());
        DISPATCH();
    }
    CASE(CloseUpvalueFrom): {
        uint16_t slot = READ_SHORT();
        closeUpvalues(slots0 + slot);
        DISPATCH();
    }
    CASE(GetProperty): {
        ObjString* name = READ_NAME();
        Value receiver = PEEK(0);
        Value key = objValue(name);
        Value result = NilValue;
        if (isInstance(receiver)) {
            ObjInstance* instance = asInstance(receiver);
            if (!instance->fields.get(key, result)) {
                RUNTIME_ERROR("kelas \"" + instance->klass->name->chars +
                              "\" teu boga sipat \"" + name->chars + "\"");
            }
        } else if (isModule(receiver)) {
            ObjModule* module = asModule(receiver);
            if (!module->globals.get(key, result)) {
                RUNTIME_ERROR("modul \"" + module->name->chars + "\" teu boga \"" +
                              name->chars + "\"");
            }
        } else {
            int tableIndex = -1;
            if (isList(receiver)) tableIndex = 0;
            else if (isMap(receiver)) tableIndex = 1;
            else if (isSet(receiver)) tableIndex = 2;
            else if (isString(receiver)) tableIndex = 3;
            else if (isTuple(receiver)) tableIndex = 4;
            if (tableIndex < 0 || !methodTables[tableIndex].get(key, result)) {
                RUNTIME_ERROR("teu boga sipat \"" + name->chars + "\" pikeun " +
                              typeName(receiver));
            }
            SYNC();
            result = objValue(newBoundMethod(heap, receiver, asObj(result)));
        }
        PEEK(0) = result;
        DISPATCH();
    }
    CASE(SetProperty): {
        ObjString* name = READ_NAME();
        Value value = PEEK(0);
        Value receiver = PEEK(1);
        if (!isInstance(receiver)) {
            RUNTIME_ERROR("teu bisa nulis sipat \"" + name->chars + "\" pikeun " +
                          typeName(receiver));
        }
        asInstance(receiver)->fields.set(objValue(name), value);
        --sp;
        DISPATCH();
    }
    CASE(GetSuper): {
        ObjString* name = READ_NAME();
        uint8_t argc = READ_BYTE();
        ObjClass* super = frames.back().closure->super;
        if (super == nullptr) RUNTIME_ERROR("teu aya induk pikeun \"" + name->chars + "\"");
        Value method;
        ObjClass* owner = nullptr;
        ObjClass* klass = super;
        while (klass != nullptr) {
            if (klass->methods.get(objValue(name), method)) {
                owner = klass;
                break;
            }
            klass = klass->superclass;
        }
        if (owner == nullptr || !isClosure(method)) {
            RUNTIME_ERROR("induk teu boga metoda \"" + name->chars + "\"");
        }
        SYNC();
        frames.back().ip = ip;
        if (!callFunctionCommon(asClosure(method), argc, true)) goto runtime_error;
        sp = stackTop;
        REFRESH_FRAME();
        ip = frames.back().ip;
        DISPATCH();
    }
    CASE(GetIndex): {
        Value index = POP();
        Value object = POP();
        Value result = NilValue;
        if (isList(object) || isTuple(object)) {
            const std::vector<Value>& items =
                isList(object) ? asList(object)->items : asTuple(object)->items;
            if (!isNumber(index)) RUNTIME_ERROR("indeks kudu angka");
            double position = asNumber(index);
            if (position < 0) position += static_cast<double>(items.size());
            if (position < 0 || static_cast<size_t>(position) >= items.size()) {
                RUNTIME_ERROR("indeks di luar rentang");
            }
            result = items[static_cast<size_t>(position)];
        } else if (isString(object)) {
            const std::string& chars = asString(object)->chars;
            if (!isNumber(index)) RUNTIME_ERROR("indeks kudu angka");
            double position = asNumber(index);
            if (position < 0) position += static_cast<double>(chars.size());
            if (position < 0 || static_cast<size_t>(position) >= chars.size()) {
                RUNTIME_ERROR("indeks di luar rentang");
            }
            size_t offset = static_cast<size_t>(position);
            unsigned char lead = static_cast<unsigned char>(chars[offset]);
            size_t length = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
            if (offset + length > chars.size()) length = chars.size() - offset;
            SYNC();
            result = objValue(copyString(heap, std::string_view(chars).substr(offset, length)));
        } else if (isMap(object)) {
            asMap(object)->table.get(index, result);
        } else if (isRange(object)) {
            ObjRange* range = asRange(object);
            if (!isNumber(index)) RUNTIME_ERROR("indeks kudu angka");
            double position = asNumber(index);
            double length = range->step > 0 ? (range->stop - range->start) / range->step
                                            : (range->start - range->stop) / -range->step;
            if (position < 0) position += length;
            if (position < 0 || position >= length) RUNTIME_ERROR("indeks di luar rentang");
            result = numberValue(range->start + position * range->step);
        } else {
            RUNTIME_ERROR("teu bisa diindeks nilai jenis " + std::string(typeName(object)));
        }
        PUSH(result);
        DISPATCH();
    }
    CASE(SetIndex): {
        Value value = POP();
        Value index = POP();
        Value object = POP();
        if (isList(object)) {
            std::vector<Value>& items = asList(object)->items;
            if (!isNumber(index)) RUNTIME_ERROR("indeks kudu angka");
            double position = asNumber(index);
            if (position < 0) position += static_cast<double>(items.size());
            if (position < 0 || static_cast<size_t>(position) >= items.size()) {
                RUNTIME_ERROR("indeks di luar rentang");
            }
            items[static_cast<size_t>(position)] = value;
        } else if (isMap(object)) {
            asMap(object)->table.set(index, value);
        } else {
            RUNTIME_ERROR("teu bisa nulis indeks pikeun " + std::string(typeName(object)));
        }
        PUSH(value);
        DISPATCH();
    }
    CASE(Slice): {
        uint8_t flags = READ_BYTE();
        Value end = PEEK(0);
        Value start = PEEK(1);
        Value object = PEEK(2);
        Value result = NilValue;
        if (isList(object) || isTuple(object) || isString(object)) {
            size_t length = isList(object) ? asList(object)->items.size()
                                           : isTuple(object) ? asTuple(object)->items.size()
                                                             : asString(object)->chars.size();
            double begin = 0;
            double finish = static_cast<double>(length);
            if (flags & 1) {
                if (!isNumber(start)) RUNTIME_ERROR("indeks awal kudu angka");
                begin = asNumber(start);
                if (begin < 0) begin += static_cast<double>(length);
            }
            if (flags & 2) {
                if (!isNumber(end)) RUNTIME_ERROR("indeks tungtung kudu angka");
                finish = asNumber(end);
                if (finish < 0) finish += static_cast<double>(length);
            }
            if (begin < 0) begin = 0;
            if (finish > static_cast<double>(length)) finish = static_cast<double>(length);
            if (finish < begin) finish = begin;
            size_t first = static_cast<size_t>(begin);
            size_t last = static_cast<size_t>(finish);
            SYNC();
            if (isList(object)) {
                ObjList* list = newList(heap);
                list->items.assign(asList(object)->items.begin() + first,
                                   asList(object)->items.begin() + last);
                result = objValue(list);
            } else if (isTuple(object)) {
                ObjTuple* tuple = newTuple(heap);
                tuple->items.assign(asTuple(object)->items.begin() + first,
                                    asTuple(object)->items.begin() + last);
                result = objValue(tuple);
            } else {
                result = objValue(copyString(
                    heap, std::string_view(asString(object)->chars).substr(first, last - first)));
            }
        } else {
            RUNTIME_ERROR("teu bisa dibeulahkeun nilai jenis " + std::string(typeName(object)));
        }
        sp -= 3;
        PUSH(result);
        DISPATCH();
    }
    CASE(List): {
        uint16_t count = READ_SHORT();
        SYNC();
        ObjList* list = newList(heap);
        list->items.reserve(count);
        for (int index = count - 1; index >= 0; --index) {
            list->items.push_back(PEEK(index));
        }
        sp -= count;
        PUSH(objValue(list));
        DISPATCH();
    }
    CASE(Map): {
        uint16_t count = READ_SHORT();
        SYNC();
        ObjMap* map = newMap(heap);
        for (int index = count - 1; index >= 0; --index) {
            Value value = PEEK(index * 2);
            Value key = PEEK(index * 2 + 1);
            map->table.set(key, value);
        }
        sp -= count * 2;
        PUSH(objValue(map));
        DISPATCH();
    }
    CASE(Tuple): {
        uint16_t count = READ_SHORT();
        SYNC();
        ObjTuple* tuple = newTuple(heap);
        tuple->items.reserve(count);
        for (int index = count - 1; index >= 0; --index) {
            tuple->items.push_back(PEEK(index));
        }
        sp -= count;
        PUSH(objValue(tuple));
        DISPATCH();
    }
    CASE(Set): {
        uint16_t count = READ_SHORT();
        SYNC();
        ObjSet* set = newSet(heap);
        for (int index = count - 1; index >= 0; --index) {
            set->table.set(PEEK(index), NilValue);
        }
        sp -= count;
        PUSH(objValue(set));
        DISPATCH();
    }
    CASE(DefineClass): {
        ObjString* name = READ_NAME();
        uint8_t methodCount = READ_BYTE();
        SYNC();
        Value superclassValue = PEEK(methodCount);
        ObjClass* superclass = nullptr;
        if (!isNil(superclassValue)) {
            if (!isClass(superclassValue)) RUNTIME_ERROR("induk teu valid");
            superclass = asClass(superclassValue);
        }
        ObjClass* klass = newClass(heap, name);
        klass->superclass = superclass;
        for (int index = methodCount - 1; index >= 0; --index) {
            Value methodValue = PEEK(index);
            if (!isClosure(methodValue)) RUNTIME_ERROR("metoda teu valid");
            ObjClosure* method = asClosure(methodValue);
            method->super = superclass;
            klass->methods.set(objValue(method->function->name), methodValue);
        }
        sp -= methodCount + 1;
        PUSH(objValue(klass));
        DISPATCH();
    }
    CASE(Jump): {
        uint16_t offset = READ_SHORT();
        ip += offset;
        DISPATCH();
    }
    CASE(JumpIfFalse): {
        uint16_t offset = READ_SHORT();
        if (isFalsey(PEEK(0))) ip += offset;
        DISPATCH();
    }
    CASE(JumpIfTrue): {
        uint16_t offset = READ_SHORT();
        if (!isFalsey(PEEK(0))) ip += offset;
        DISPATCH();
    }
    CASE(Loop): {
        uint16_t offset = READ_SHORT();
        ip -= offset;
        DISPATCH();
    }
    CASE(IterNext): {
        uint16_t slot = READ_SHORT();
        uint16_t offset = READ_SHORT();
        SYNC();
        Value element = NilValue;
        int status = iterate(slots0 + slot, element);
        if (status < 0) {
            Value iterable = slots0[slot];
            RUNTIME_ERROR("teu bisa diulang nilai jenis " + std::string(typeName(iterable)));
        }
        if (status == 0) ip += offset;
        else PUSH(element);
        DISPATCH();
    }
    CASE(Equal): {
        Value b = POP();
        Value a = POP();
        PUSH(boolValue(valuesEqual(a, b)));
        DISPATCH();
    }
    CASE(Greater): {
        BINARY_COMPARE(a > b);
        DISPATCH();
    }
    CASE(Less): {
        BINARY_COMPARE(a < b);
        DISPATCH();
    }
    CASE(Add): {
        Value right = PEEK(0);
        Value left = PEEK(1);
        if (isNumber(left) && isNumber(right)) {
            --sp;
            sp[-1] = numberValue(asNumber(left) + asNumber(right));
        } else if (isString(left) && isString(right)) {
            SYNC();
            ObjString* result = concatenate(heap, left, right);
            sp -= 2;
            PUSH(objValue(result));
        } else {
            RUNTIME_ERROR("teu bisa dijumlahkeun " + std::string(typeName(left)) + " jeung " +
                          std::string(typeName(right)));
        }
        DISPATCH();
    }
    CASE(Subtract): {
        BINARY_NUMBER(a - b, "teu bisa dikurangkeun");
        DISPATCH();
    }
    CASE(Multiply): {
        BINARY_NUMBER(a * b, "teu bisa dikalikeun");
        DISPATCH();
    }
    CASE(Divide): {
        if (!isNumber(PEEK(0)) || !isNumber(PEEK(1))) RUNTIME_ERROR("teu bisa dibagikeun");
        if (asNumber(PEEK(0)) == 0) RUNTIME_ERROR("dibagi nol");
        double b = asNumber(POP());
        double a = asNumber(POP());
        PUSH(numberValue(a / b));
        DISPATCH();
    }
    CASE(Modulo): {
        if (!isNumber(PEEK(0)) || !isNumber(PEEK(1))) RUNTIME_ERROR("teu bisa dibagikeun");
        if (asNumber(PEEK(0)) == 0) RUNTIME_ERROR("dibagi nol");
        double b = asNumber(POP());
        double a = asNumber(POP());
        PUSH(numberValue(std::fmod(a, b)));
        DISPATCH();
    }
    CASE(Power): {
        BINARY_NUMBER(std::pow(a, b), "teu bisa dipangkatkeun");
        DISPATCH();
    }
    CASE(Not): {
        PEEK(0) = boolValue(isFalsey(PEEK(0)));
        DISPATCH();
    }
    CASE(Negate): {
        if (!isNumber(PEEK(0))) RUNTIME_ERROR("teu bisa dinegatifkeun");
        PEEK(0) = numberValue(-asNumber(PEEK(0)));
        DISPATCH();
    }
    CASE(Call): {
        uint8_t argc = READ_BYTE();
        Value callee = PEEK(argc);
        if (!isCallable(callee)) {
            RUNTIME_ERROR("teu bisa dipanggil nilai jenis " + std::string(typeName(callee)));
        }
        SYNC();
        raiseIp = ip;
        raiseClosure = frames.back().closure;
        frames.back().ip = ip;
        if (!callCallable(callee, argc)) goto runtime_error;
        sp = stackTop;
        REFRESH_FRAME();
        ip = frames.back().ip;
        DISPATCH();
    }
    CASE(Invoke): {
        ObjString* name = READ_NAME();
        uint8_t argc = READ_BYTE();
        Value receiver = PEEK(argc);
        SYNC();
        raiseIp = ip;
        raiseClosure = frames.back().closure;
        frames.back().ip = ip;
        invoke(receiver, name, argc);
        if (!pendingError.empty()) goto runtime_error;
        sp = stackTop;
        REFRESH_FRAME();
        ip = frames.back().ip;
        DISPATCH();
    }
    CASE(Closure): {
        Value functionValue = constants[READ_SHORT()];
        uint8_t count = READ_BYTE();
        SYNC();
        ObjFunction* function = asFunction(functionValue);
        ObjClosure* closure = newClosure(heap, function);
        heap.protect(objValue(closure));
            for (int index = 0; index < count; ++index) {
            bool isLocal = READ_BYTE() != 0;
            uint16_t upvalueIndex = READ_BYTE();
            closure->upvalues[index] =
                isLocal ? captureUpvalue(slots0 + upvalueIndex)
                        : frames.back().closure->upvalues[upvalueIndex];
        }
        heap.unprotect();
        PUSH(objValue(closure));
        DISPATCH();
    }
    CASE(Import): {
        ObjString* name = READ_NAME();
        uint8_t kind = READ_BYTE();
        uint16_t dest = READ_SHORT();
        SYNC();
        Value moduleValue = NilValue;
        bool started = false;
        raiseIp = ip;
        raiseClosure = frames.back().closure;
        frames.back().ip = ip;
        ObjModule* importer = frames.back().module;
        if (!loadModule(name->chars, moduleValue, started)) goto runtime_error;
        if (kind == 0) importer->globals.set(objValue(name), moduleValue);
        else slots0[dest] = moduleValue;
        if (started) {
            sp = stackTop;
            REFRESH_FRAME();
            ip = frames.back().ip;
        }
        DISPATCH();
    }
    CASE(Try): {
        uint16_t offset = READ_SHORT();
        frames.back().catches.push_back(CatchPoint{ip + offset, sp});
        DISPATCH();
    }
    CASE(TryEnd): {
        if (!frames.back().catches.empty()) frames.back().catches.pop_back();
        DISPATCH();
    }
    CASE(Throw): {
        USER_THROW(POP());
    }
    CASE(Echo): {
        echoValue(PEEK(0));
        --sp;
        DISPATCH();
    }
    CASE(Return): {
        CallFrame& frame = frames.back();
        Value result = frame.isModule ? NilValue : POP();
        bool wasModule = frame.isModule;
        bool wasInit = frame.isInit;
        Value* frameSlots = frame.slots;
        closeUpvalues(frameSlots);
        frames.pop_back();
        sp = frameSlots;
        if (frames.empty()) {
            return InterpretResult::Ok;
        }
        if (wasModule) {
        } else if (wasInit) {
            PUSH(frameSlots[0]);
        } else {
            PUSH(result);
        }
        REFRESH_FRAME();
        ip = frames.back().ip;
        DISPATCH();
    }

    LOOP_END

runtime_error:
user_throw: {
        raiseIp = ip;
        raiseClosure = frames.back().closure;
        SYNC();
        Value errorValue = pendingError.empty()
                               ? thrownValue
                               : objValue(copyString(heap, pendingError));
        pendingError.clear();
        thrownValue = NilValue;
        bool handled = false;
        while (!frames.empty()) {
            CallFrame& frame = frames.back();
            if (!frame.catches.empty()) {
                CatchPoint point = frame.catches.back();
                frame.catches.pop_back();
                ip = point.ip;
                sp = point.sp;
                PUSH(errorValue);
                REFRESH_FRAME();
                handled = true;
                break;
            }
            closeUpvalues(frame.slots);
            frames.pop_back();
        }
        if (handled) DISPATCH();
        reportError(raiseIp, raiseClosure, errorValue);
        stackTop = stack.get();
        return InterpretResult::RuntimeError;
    }

#undef PUSH
#undef POP
#undef PEEK
#undef READ_BYTE
#undef READ_SHORT
#undef READ_NAME
#undef SYNC
#undef RUNTIME_ERROR
#undef USER_THROW
#undef BINARY_NUMBER
#undef BINARY_COMPARE
#undef REFRESH_FRAME
#undef CASE
#undef DISPATCH
#undef LOOP_BEGIN
#undef LOOP_END
}

#pragma GCC diagnostic pop

}
