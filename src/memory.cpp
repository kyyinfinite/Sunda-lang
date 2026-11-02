#include "memory.h"

#include <cstdlib>

#include "vm.h"

namespace sunda {

namespace {

std::vector<Obj*>& grayStack() {
    static std::vector<Obj*> stack;
    return stack;
}

bool isDeadString(Value key) {
    return isString(key) && !asString(key)->marked;
}

void markObjectImpl(Obj* object);

void markValueImpl(Value value) {
    if (isObj(value)) markObjectImpl(asObj(value));
}

void markObjectImpl(Obj* object) {
    if (object == nullptr || object->marked) return;
    object->marked = true;
    grayStack().push_back(object);
}

void markTableImpl(Table& table) {
    for (const Entry& entry : table.raw()) {
        if (entry.key.bits == EmptyKeyValue.bits || entry.key.bits == TombstoneValue.bits) continue;
        markValueImpl(entry.key);
        markValueImpl(entry.value);
    }
}

void blackenObject(Obj* object) {
    switch (object->type) {
        case ObjType::String:
        case ObjType::Range:
            break;
        case ObjType::Native: {
            ObjNative* native = static_cast<ObjNative*>(object);
            markObjectImpl(native->name);
            break;
        }
        case ObjType::Upvalue: {
            ObjUpvalue* upvalue = static_cast<ObjUpvalue*>(object);
            markValueImpl(upvalue->closed);
            break;
        }
        case ObjType::Function: {
            ObjFunction* function = static_cast<ObjFunction*>(object);
            markObjectImpl(function->name);
            for (Value value : function->chunk.constants) markValueImpl(value);
            break;
        }
        case ObjType::Closure: {
            ObjClosure* closure = static_cast<ObjClosure*>(object);
            markObjectImpl(closure->function);
            markObjectImpl(closure->super);
            for (ObjUpvalue* upvalue : closure->upvalues) markObjectImpl(upvalue);
            break;
        }
        case ObjType::Class: {
            ObjClass* klass = static_cast<ObjClass*>(object);
            markObjectImpl(klass->name);
            markObjectImpl(klass->superclass);
            markTableImpl(klass->methods);
            break;
        }
        case ObjType::Instance: {
            ObjInstance* instance = static_cast<ObjInstance*>(object);
            markObjectImpl(instance->klass);
            markTableImpl(instance->fields);
            break;
        }
        case ObjType::BoundMethod: {
            ObjBoundMethod* bound = static_cast<ObjBoundMethod*>(object);
            markValueImpl(bound->receiver);
            markObjectImpl(bound->method);
            break;
        }
        case ObjType::List:
            for (Value value : static_cast<ObjList*>(object)->items) markValueImpl(value);
            break;
        case ObjType::Tuple:
            for (Value value : static_cast<ObjTuple*>(object)->items) markValueImpl(value);
            break;
        case ObjType::Map:
            markTableImpl(static_cast<ObjMap*>(object)->table);
            break;
        case ObjType::Set:
            markTableImpl(static_cast<ObjSet*>(object)->table);
            break;
        case ObjType::Module: {
            ObjModule* module = static_cast<ObjModule*>(object);
            markObjectImpl(module->name);
            markTableImpl(module->globals);
            break;
        }
    }
}

void sweep(Heap& heap) {
    Obj* previous = nullptr;
    Obj* object = heap.objects;
    while (object != nullptr) {
        if (object->marked) {
            object->marked = false;
            previous = object;
            object = object->next;
        } else {
            Obj* unreached = object;
            object = object->next;
            if (previous == nullptr) heap.objects = object;
            else previous->next = object;
            freeObject(heap, unreached);
        }
    }
}

}

void collectGarbage(VM& vm) {
    Heap& heap = vm.heap;
    heap.pauseGC();

    for (Value* slot = vm.stack.get(); slot < vm.stackTop; ++slot) markValueImpl(*slot);
    for (CallFrame& frame : vm.frames) {
        markObjectImpl(frame.closure);
        if (frame.module != nullptr) {
            markObjectImpl(frame.module->name);
            markTableImpl(frame.module->globals);
        }
    }
    markTableImpl(vm.modules);
    markTableImpl(vm.builtins);
    for (Table& table : vm.methodTables) markTableImpl(table);
    for (ObjUpvalue* upvalue = heap.openUpvalues; upvalue != nullptr; upvalue = upvalue->openNext) {
        markObjectImpl(upvalue);
    }
    for (Value value : heap.tempRoots) markValueImpl(value);

    while (!grayStack().empty()) {
        Obj* object = grayStack().back();
        grayStack().pop_back();
        blackenObject(object);
    }

    heap.strings.removeIf(isDeadString);
    sweep(heap);

    size_t next = heap.bytesAllocated * 2;
    heap.nextGC = next < 1024 * 1024 ? 1024 * 1024 : next;
    heap.resumeGC();
}

void freeObject(Heap& heap, Obj* object) {
    switch (object->type) {
        case ObjType::String: {
            ObjString* string = static_cast<ObjString*>(object);
            heap.bytesAllocated -= sizeof(ObjString) + string->chars.capacity();
            delete string;
            return;
        }
        case ObjType::Native:
            heap.bytesAllocated -= sizeof(ObjNative);
            delete static_cast<ObjNative*>(object);
            return;
        case ObjType::Upvalue:
            heap.bytesAllocated -= sizeof(ObjUpvalue);
            delete static_cast<ObjUpvalue*>(object);
            return;
        case ObjType::Function:
            heap.bytesAllocated -= sizeof(ObjFunction);
            delete static_cast<ObjFunction*>(object);
            return;
        case ObjType::Closure:
            heap.bytesAllocated -= sizeof(ObjClosure);
            delete static_cast<ObjClosure*>(object);
            return;
        case ObjType::Class:
            heap.bytesAllocated -= sizeof(ObjClass);
            delete static_cast<ObjClass*>(object);
            return;
        case ObjType::Instance:
            heap.bytesAllocated -= sizeof(ObjInstance);
            delete static_cast<ObjInstance*>(object);
            return;
        case ObjType::BoundMethod:
            heap.bytesAllocated -= sizeof(ObjBoundMethod);
            delete static_cast<ObjBoundMethod*>(object);
            return;
        case ObjType::List:
            heap.bytesAllocated -= sizeof(ObjList);
            delete static_cast<ObjList*>(object);
            return;
        case ObjType::Map:
            heap.bytesAllocated -= sizeof(ObjMap);
            delete static_cast<ObjMap*>(object);
            return;
        case ObjType::Set:
            heap.bytesAllocated -= sizeof(ObjSet);
            delete static_cast<ObjSet*>(object);
            return;
        case ObjType::Tuple:
            heap.bytesAllocated -= sizeof(ObjTuple);
            delete static_cast<ObjTuple*>(object);
            return;
        case ObjType::Range:
            heap.bytesAllocated -= sizeof(ObjRange);
            delete static_cast<ObjRange*>(object);
            return;
        case ObjType::Module:
            heap.bytesAllocated -= sizeof(ObjModule);
            delete static_cast<ObjModule*>(object);
            return;
    }
}

Heap::~Heap() {
    Obj* object = objects;
    while (object != nullptr) {
        Obj* next = object->next;
        switch (object->type) {
            case ObjType::String: delete static_cast<ObjString*>(object); break;
            case ObjType::Native: delete static_cast<ObjNative*>(object); break;
            case ObjType::Upvalue: delete static_cast<ObjUpvalue*>(object); break;
            case ObjType::Function: delete static_cast<ObjFunction*>(object); break;
            case ObjType::Closure: delete static_cast<ObjClosure*>(object); break;
            case ObjType::Class: delete static_cast<ObjClass*>(object); break;
            case ObjType::Instance: delete static_cast<ObjInstance*>(object); break;
            case ObjType::BoundMethod: delete static_cast<ObjBoundMethod*>(object); break;
            case ObjType::List: delete static_cast<ObjList*>(object); break;
            case ObjType::Map: delete static_cast<ObjMap*>(object); break;
            case ObjType::Set: delete static_cast<ObjSet*>(object); break;
            case ObjType::Tuple: delete static_cast<ObjTuple*>(object); break;
            case ObjType::Range: delete static_cast<ObjRange*>(object); break;
            case ObjType::Module: delete static_cast<ObjModule*>(object); break;
        }
        object = next;
    }
    objects = nullptr;
    openUpvalues = nullptr;
}

}
