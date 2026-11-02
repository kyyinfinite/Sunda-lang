#include "object.h"

#include <cmath>
#include <cstdio>

#include "memory.h"

namespace sunda {

namespace {

std::string formatNumber(double number) {
    if (std::isnan(number)) return "nan";
    if (std::isinf(number)) return number < 0 ? "-inf" : "inf";
    if (number == static_cast<long long>(number) && number < 1e15 && number > -1e15) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%lld", static_cast<long long>(number));
        return buffer;
    }
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.14g", number);
    return buffer;
}

ObjString* allocateString(Heap& heap, std::string&& chars, uint32_t hash) {
    ObjString* string = allocateObject<ObjString>(heap, ObjType::String);
    string->chars = std::move(chars);
    string->hash = hash;
    heap.strings.set(objValue(string), NilValue);
    heap.bytesAllocated += string->chars.capacity();
    return string;
}

void reprValue(std::string& out, Value value);

void reprItems(std::string& out, const std::vector<Value>& items) {
    for (size_t index = 0; index < items.size(); ++index) {
        if (index > 0) out += ", ";
        reprValue(out, items[index]);
    }
}

void reprValue(std::string& out, Value value) {
    if (isString(value)) {
        out += '"';
        for (char c : asString(value)->chars) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\t': out += "\\t"; break;
                case '\r': out += "\\r"; break;
                default: out += c; break;
            }
        }
        out += '"';
        return;
    }
    if (isList(value)) {
        out += '[';
        reprItems(out, asList(value)->items);
        out += ']';
        return;
    }
    if (isTuple(value)) {
        out += '(';
        reprItems(out, asTuple(value)->items);
        if (asTuple(value)->items.size() == 1) out += ',';
        out += ')';
        return;
    }
    if (isMap(value)) {
        out += '{';
        bool first = true;
        for (const Entry& entry : asMap(value)->table.raw()) {
            if (entry.key.bits == EmptyKeyValue.bits || entry.key.bits == TombstoneValue.bits) continue;
            if (!first) out += ", ";
            first = false;
            reprValue(out, entry.key);
            out += ": ";
            reprValue(out, entry.value);
        }
        out += '}';
        return;
    }
    if (isSet(value)) {
        out += '{';
        bool first = true;
        for (const Entry& entry : asSet(value)->table.raw()) {
            if (entry.key.bits == EmptyKeyValue.bits || entry.key.bits == TombstoneValue.bits) continue;
            if (!first) out += ", ";
            first = false;
            reprValue(out, entry.key);
        }
        out += '}';
        return;
    }
    if (isRange(value)) {
        ObjRange* range = asRange(value);
        out += "rentang(" + formatNumber(range->start) + ", " + formatNumber(range->stop);
        if (range->step != 1) out += ", " + formatNumber(range->step);
        out += ')';
        return;
    }
    if (isClass(value)) {
        out += "<kelas " + asClass(value)->name->chars + ">";
        return;
    }
    if (isInstance(value)) {
        out += "<objek " + asInstance(value)->klass->name->chars + ">";
        return;
    }
    if (isClosure(value)) {
        out += "<pagawean " + asClosure(value)->function->name->chars + ">";
        return;
    }
    if (isBoundMethod(value)) {
        ObjBoundMethod* bound = asBoundMethod(value);
        out += "<metoda " + valueToString(bound->receiver) + ">";
        return;
    }
    if (isModule(value)) {
        out += "<modul " + asModule(value)->name->chars + ">";
        return;
    }
    out += valueToString(value);
}

}

uint32_t hashString(std::string_view chars) {
    uint32_t hash = 2166136261u;
    for (unsigned char c : chars) {
        hash ^= c;
        hash *= 16777619u;
    }
    return hash;
}

ObjString* copyString(Heap& heap, std::string_view chars) {
    uint32_t hash = hashString(chars);
    ObjString* interned = heap.strings.findString(chars, hash);
    if (interned != nullptr) return interned;
    return allocateString(heap, std::string(chars), hash);
}

ObjString* takeString(Heap& heap, std::string&& chars) {
    uint32_t hash = hashString(chars);
    ObjString* interned = heap.strings.findString(chars, hash);
    if (interned != nullptr) return interned;
    return allocateString(heap, std::move(chars), hash);
}

ObjList* newList(Heap& heap) { return allocateObject<ObjList>(heap, ObjType::List); }
ObjMap* newMap(Heap& heap) { return allocateObject<ObjMap>(heap, ObjType::Map); }
ObjSet* newSet(Heap& heap) { return allocateObject<ObjSet>(heap, ObjType::Set); }
ObjTuple* newTuple(Heap& heap) { return allocateObject<ObjTuple>(heap, ObjType::Tuple); }

ObjRange* newRange(Heap& heap, double start, double stop, double step) {
    ObjRange* range = allocateObject<ObjRange>(heap, ObjType::Range);
    range->start = start;
    range->stop = stop;
    range->step = step;
    return range;
}

ObjClass* newClass(Heap& heap, ObjString* name) {
    ObjClass* klass = allocateObject<ObjClass>(heap, ObjType::Class);
    klass->name = name;
    return klass;
}

ObjInstance* newInstance(Heap& heap, ObjClass* klass) {
    ObjInstance* instance = allocateObject<ObjInstance>(heap, ObjType::Instance);
    instance->klass = klass;
    return instance;
}

ObjBoundMethod* newBoundMethod(Heap& heap, Value receiver, Obj* method) {
    ObjBoundMethod* bound = allocateObject<ObjBoundMethod>(heap, ObjType::BoundMethod);
    bound->receiver = receiver;
    bound->method = method;
    return bound;
}

ObjModule* newModule(Heap& heap, ObjString* name) {
    ObjModule* module = allocateObject<ObjModule>(heap, ObjType::Module);
    module->name = name;
    return module;
}

ObjClosure* newClosure(Heap& heap, ObjFunction* function) {
    ObjClosure* closure = allocateObject<ObjClosure>(heap, ObjType::Closure);
    closure->function = function;
    closure->upvalues.assign(function->upvalueCount, nullptr);
    return closure;
}

bool isCallable(Value value) {
    return isNative(value) || isClosure(value) || isBoundMethod(value) || isClass(value);
}

std::string valueToString(Value value) {
    if (isNumber(value)) return formatNumber(asNumber(value));
    if (isNil(value)) return "kosong";
    if (isBool(value)) return asBool(value) ? "bener" : "salah";
    if (isString(value)) return asString(value)->chars;
    if (isNative(value)) return "<pagawean " + asNative(value)->name->chars + ">";
    if (isFunction(value)) return "<pagawean " + asFunction(value)->name->chars + ">";
    if (isList(value)) {
        std::string out = "[";
        reprItems(out, asList(value)->items);
        out += ']';
        return out;
    }
    if (isTuple(value)) {
        std::string out = "(";
        reprItems(out, asTuple(value)->items);
        if (asTuple(value)->items.size() == 1) out += ',';
        out += ')';
        return out;
    }
    if (isMap(value) || isSet(value) || isRange(value) || isClass(value) ||
        isInstance(value) || isClosure(value) || isBoundMethod(value) || isModule(value)) {
        std::string out;
        reprValue(out, value);
        return out;
    }
    return "<objek>";
}

const char* typeName(Value value) {
    if (isNumber(value)) return "angka";
    if (isNil(value)) return "kosong";
    if (isBool(value)) return "boolean";
    if (isString(value)) return "kecap";
    if (isList(value)) return "daptar";
    if (isMap(value)) return "kamus";
    if (isSet(value)) return "himpunan";
    if (isTuple(value)) return "tuple";
    if (isRange(value)) return "rentang";
    if (isClass(value)) return "kelas";
    if (isInstance(value)) return "objek";
    if (isModule(value)) return "modul";
    return "pagawean";
}

}
