#pragma once

#include <cstddef>
#include <vector>

#include "object.h"

namespace sunda {

struct VM;

struct Heap {
    Obj* objects = nullptr;
    ObjUpvalue* openUpvalues = nullptr;
    Table strings;
    size_t bytesAllocated = 0;
    size_t nextGC = 1024 * 1024;
    int gcDepth = 0;
    bool stressGC = false;
    VM* vm = nullptr;
    std::vector<Value> tempRoots;

    Heap() = default;
    Heap(const Heap&) = delete;
    Heap& operator=(const Heap&) = delete;
    ~Heap();

    void pauseGC() { ++gcDepth; }
    void resumeGC() { --gcDepth; }
    void protect(Value value) { tempRoots.push_back(value); }
    void unprotect() { tempRoots.pop_back(); }
};

void collectGarbage(VM& vm);
void freeObject(Heap& heap, Obj* object);

template <typename T>
T* allocateObject(Heap& heap, ObjType type) {
    if (heap.gcDepth == 0 && heap.vm != nullptr &&
        (heap.stressGC || heap.bytesAllocated >= heap.nextGC)) {
        collectGarbage(*heap.vm);
    }
    T* object = new T();
    object->type = type;
    object->next = heap.objects;
    heap.objects = object;
    heap.bytesAllocated += sizeof(T);
    return object;
}

}
