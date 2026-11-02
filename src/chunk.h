#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "value.h"

namespace sunda {

#define SUNDA_OPCODES(X) \
    X(Constant)          \
    X(Nil)               \
    X(True)              \
    X(False)             \
    X(Pop)               \
    X(PopN)              \
    X(Dup)               \
    X(Dup2)              \
    X(GetLocal)          \
    X(SetLocal)          \
    X(GetGlobal)         \
    X(SetGlobal)         \
    X(GetUpvalue)        \
    X(SetUpvalue)        \
    X(CloseUpvalueN)     \
    X(CloseUpvalueFrom)  \
    X(GetProperty)       \
    X(SetProperty)       \
    X(GetSuper)          \
    X(DefineClass)       \
    X(GetIndex)          \
    X(SetIndex)          \
    X(Slice)             \
    X(List)              \
    X(Map)               \
    X(Tuple)             \
    X(Set)               \
    X(Jump)              \
    X(JumpIfFalse)       \
    X(JumpIfTrue)        \
    X(Loop)              \
    X(IterNext)          \
    X(Equal)             \
    X(Greater)           \
    X(Less)              \
    X(Add)               \
    X(Subtract)          \
    X(Multiply)          \
    X(Divide)            \
    X(Modulo)            \
    X(Power)             \
    X(Not)               \
    X(Negate)            \
    X(Call)              \
    X(Invoke)            \
    X(Closure)           \
    X(Import)            \
    X(Try)               \
    X(TryEnd)            \
    X(Throw)             \
    X(Echo)              \
    X(Return)

enum class OpCode : uint8_t {
#define X(name) name,
    SUNDA_OPCODES(X)
#undef X
};

struct Chunk {
    std::vector<uint8_t> code;
    std::vector<int> lines;
    std::vector<Value> constants;

    void write(uint8_t byte, int line);
    size_t addConstant(Value value);
};

}
