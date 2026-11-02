#pragma once

#include <cstdint>
#include <cstring>

namespace sunda {

struct Obj;

struct Value {
    uint64_t bits;
};

inline constexpr uint64_t QnanMask = 0x7ffc000000000000ULL;
inline constexpr uint64_t SignBit = 0x8000000000000000ULL;
inline constexpr uint64_t PointerMask = 0x0000ffffffffffffULL;

inline constexpr uint64_t TagNil = 1;
inline constexpr uint64_t TagFalse = 2;
inline constexpr uint64_t TagTrue = 3;
inline constexpr uint64_t TagEmptyKey = 4;
inline constexpr uint64_t TagTombstone = 5;

inline constexpr Value NilValue{QnanMask | TagNil};
inline constexpr Value FalseValue{QnanMask | TagFalse};
inline constexpr Value TrueValue{QnanMask | TagTrue};
inline constexpr Value EmptyKeyValue{QnanMask | TagEmptyKey};
inline constexpr Value TombstoneValue{QnanMask | TagTombstone};

inline Value numberValue(double number) {
    Value value;
    std::memcpy(&value.bits, &number, sizeof(double));
    return value;
}

inline double asNumber(Value value) {
    double number;
    std::memcpy(&number, &value.bits, sizeof(double));
    return number;
}

inline bool isNumber(Value value) { return (value.bits & QnanMask) != QnanMask; }
inline bool isNil(Value value) { return value.bits == NilValue.bits; }
inline bool isBool(Value value) { return (value.bits | 1) == TrueValue.bits; }
inline bool asBool(Value value) { return value.bits == TrueValue.bits; }
inline Value boolValue(bool flag) { return flag ? TrueValue : FalseValue; }
inline bool isObj(Value value) { return (value.bits & (QnanMask | SignBit)) == (QnanMask | SignBit); }

inline Value objValue(Obj* object) {
    return Value{SignBit | QnanMask | (reinterpret_cast<uintptr_t>(object) & PointerMask)};
}

inline Obj* asObj(Value value) {
    return reinterpret_cast<Obj*>(static_cast<uintptr_t>(value.bits & PointerMask));
}

inline bool isFalsey(Value value) {
    return value.bits == NilValue.bits || value.bits == FalseValue.bits;
}

inline bool valuesEqual(Value a, Value b) {
    if (isNumber(a) && isNumber(b)) return asNumber(a) == asNumber(b);
    return a.bits == b.bits;
}

}
