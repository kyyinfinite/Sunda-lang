#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "chunk.h"
#include "native.h"
#include "table.h"
#include "value.h"

namespace sunda {

struct Heap;
struct VM;
struct ObjClass;

enum class ObjType : uint8_t {
    String,
    Native,
    Upvalue,
    Function,
    Closure,
    Class,
    Instance,
    BoundMethod,
    List,
    Map,
    Set,
    Tuple,
    Range,
    Module,
};

struct Obj {
    ObjType type = ObjType::String;
    bool marked = false;
    Obj* next = nullptr;
};

struct ObjString : Obj {
    std::string chars;
    uint32_t hash = 0;
};

struct ObjNative : Obj {
    NativeFn function = nullptr;
    int arity = 0;
    ObjString* name = nullptr;
};

struct ObjUpvalue : Obj {
    Value* location = nullptr;
    Value closed{0};
    ObjUpvalue* openNext = nullptr;
};

struct ObjFunction : Obj {
    ObjString* name = nullptr;
    int arityMin = 0;
    int arityMax = -1;
    int paramSlots = 0;
    int restSlot = -1;
    int firstDefaultSlot = -1;
    int slotCount = 0;
    int upvalueCount = 0;
    bool isMethod = false;
    std::string_view source;
    Chunk chunk;
};

struct ObjClosure : Obj {
    ObjFunction* function = nullptr;
    std::vector<ObjUpvalue*> upvalues;
    ObjClass* super = nullptr;
};

struct ObjClass : Obj {
    ObjString* name = nullptr;
    ObjClass* superclass = nullptr;
    Table methods;
};

struct ObjInstance : Obj {
    ObjClass* klass = nullptr;
    Table fields;
};

struct ObjBoundMethod : Obj {
    Value receiver{0};
    Obj* method = nullptr;
};

struct ObjList : Obj {
    std::vector<Value> items;
};

struct ObjMap : Obj {
    Table table;
};

struct ObjSet : Obj {
    Table table;
};

struct ObjTuple : Obj {
    std::vector<Value> items;
};

struct ObjRange : Obj {
    double start = 0;
    double stop = 0;
    double step = 1;
};

struct ObjModule : Obj {
    ObjString* name = nullptr;
    std::string dir;
    Table globals;
};

inline bool isObjType(Value value, ObjType type) { return isObj(value) && asObj(value)->type == type; }
inline bool isString(Value value) { return isObjType(value, ObjType::String); }
inline bool isNative(Value value) { return isObjType(value, ObjType::Native); }
inline bool isFunction(Value value) { return isObjType(value, ObjType::Function); }
inline bool isClosure(Value value) { return isObjType(value, ObjType::Closure); }
inline bool isClass(Value value) { return isObjType(value, ObjType::Class); }
inline bool isInstance(Value value) { return isObjType(value, ObjType::Instance); }
inline bool isBoundMethod(Value value) { return isObjType(value, ObjType::BoundMethod); }
inline bool isList(Value value) { return isObjType(value, ObjType::List); }
inline bool isMap(Value value) { return isObjType(value, ObjType::Map); }
inline bool isSet(Value value) { return isObjType(value, ObjType::Set); }
inline bool isTuple(Value value) { return isObjType(value, ObjType::Tuple); }
inline bool isRange(Value value) { return isObjType(value, ObjType::Range); }
inline bool isModule(Value value) { return isObjType(value, ObjType::Module); }
inline ObjString* asString(Value value) { return static_cast<ObjString*>(asObj(value)); }
inline ObjNative* asNative(Value value) { return static_cast<ObjNative*>(asObj(value)); }
inline ObjFunction* asFunction(Value value) { return static_cast<ObjFunction*>(asObj(value)); }
inline ObjClosure* asClosure(Value value) { return static_cast<ObjClosure*>(asObj(value)); }
inline ObjClass* asClass(Value value) { return static_cast<ObjClass*>(asObj(value)); }
inline ObjInstance* asInstance(Value value) { return static_cast<ObjInstance*>(asObj(value)); }
inline ObjBoundMethod* asBoundMethod(Value value) { return static_cast<ObjBoundMethod*>(asObj(value)); }
inline ObjList* asList(Value value) { return static_cast<ObjList*>(asObj(value)); }
inline ObjMap* asMap(Value value) { return static_cast<ObjMap*>(asObj(value)); }
inline ObjSet* asSet(Value value) { return static_cast<ObjSet*>(asObj(value)); }
inline ObjTuple* asTuple(Value value) { return static_cast<ObjTuple*>(asObj(value)); }
inline ObjRange* asRange(Value value) { return static_cast<ObjRange*>(asObj(value)); }
inline ObjModule* asModule(Value value) { return static_cast<ObjModule*>(asObj(value)); }

uint32_t hashString(std::string_view chars);
ObjString* copyString(Heap& heap, std::string_view chars);
ObjString* takeString(Heap& heap, std::string&& chars);

ObjList* newList(Heap& heap);
ObjMap* newMap(Heap& heap);
ObjSet* newSet(Heap& heap);
ObjTuple* newTuple(Heap& heap);
ObjRange* newRange(Heap& heap, double start, double stop, double step);
ObjClass* newClass(Heap& heap, ObjString* name);
ObjInstance* newInstance(Heap& heap, ObjClass* klass);
ObjBoundMethod* newBoundMethod(Heap& heap, Value receiver, Obj* method);
ObjModule* newModule(Heap& heap, ObjString* name);
ObjClosure* newClosure(Heap& heap, ObjFunction* function);

bool isCallable(Value value);
std::string valueToString(Value value);
std::string valueRepr(Value value);
const char* typeName(Value value);

}
