#include "native.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

#include "memory.h"
#include "modules.h"
#include "vm.h"

namespace sunda {

namespace {

Value nativeCetak(VM&, Value, int argc, Value* args) {
    std::string line;
    for (int index = 0; index < argc; ++index) {
        if (index > 0) line += ' ';
        line += valueToString(args[index]);
    }
    line += '\n';
    std::fwrite(line.data(), 1, line.size(), stdout);
    return NilValue;
}

Value nativeTulis(VM&, Value, int argc, Value* args) {
    std::string line;
    for (int index = 0; index < argc; ++index) {
        if (index > 0) line += ' ';
        line += valueToString(args[index]);
    }
    std::fwrite(line.data(), 1, line.size(), stdout);
    return NilValue;
}

Value nativeJenis(VM& vm, Value, int, Value* args) {
    return objValue(copyString(vm.heap, typeName(args[0])));
}

Value nativeKecap(VM& vm, Value, int, Value* args) {
    return objValue(takeString(vm.heap, valueToString(args[0])));
}

Value nativeAngka(VM& vm, Value, int, Value* args) {
    Value value = args[0];
    if (isNumber(value)) return value;
    if (isBool(value)) return numberValue(asBool(value) ? 1 : 0);
    if (isString(value)) {
        const std::string& text = asString(value)->chars;
        char* end = nullptr;
        double number = std::strtod(text.c_str(), &end);
        bool parsed = !text.empty() && end != text.c_str();
        while (parsed && *end != '\0' && std::isspace(static_cast<unsigned char>(*end))) ++end;
        if (parsed && *end == '\0') return numberValue(number);
    }
    vm.raise("teu bisa dijadikeun angka");
    return NilValue;
}

Value nativePanjang(VM& vm, Value, int, Value* args) {
    Value value = args[0];
    if (isString(value)) {
        double length = 0;
        for (unsigned char byte : asString(value)->chars) {
            if ((byte & 0xC0) != 0x80) ++length;
        }
        return numberValue(length);
    }
    if (isList(value)) return numberValue(static_cast<double>(asList(value)->items.size()));
    if (isTuple(value)) return numberValue(static_cast<double>(asTuple(value)->items.size()));
    if (isMap(value)) return numberValue(static_cast<double>(asMap(value)->table.liveCount()));
    if (isSet(value)) return numberValue(static_cast<double>(asSet(value)->table.liveCount()));
    if (isRange(value)) {
        ObjRange* range = asRange(value);
        double span = range->step > 0 ? range->stop - range->start : range->start - range->stop;
        double stride = range->step > 0 ? range->step : -range->step;
        if (span <= 0) return numberValue(0);
        return numberValue(std::ceil(span / stride));
    }
    vm.raise("teu boga panjang");
    return NilValue;
}

Value nativeRentang(VM& vm, Value, int argc, Value* args) {
    double start = 0;
    double stop = 0;
    double step = 1;
    if (argc == 1) {
        if (!isNumber(args[0])) {
            vm.raise("rentang kudu angka");
            return NilValue;
        }
        stop = asNumber(args[0]);
    } else if (argc == 2 || argc == 3) {
        if (!isNumber(args[0]) || !isNumber(args[1]) || (argc == 3 && !isNumber(args[2]))) {
            vm.raise("rentang kudu angka");
            return NilValue;
        }
        start = asNumber(args[0]);
        stop = asNumber(args[1]);
        if (argc == 3) step = asNumber(args[2]);
    } else {
        vm.raise("rentang butuh 1-3 argumen");
        return NilValue;
    }
    if (step == 0) {
        vm.raise("rentang teu bisa boga lankah nol");
        return NilValue;
    }
    return objValue(newRange(vm.heap, start, stop, step));
}

Value nativeAsupan(VM& vm, Value, int, Value*) {
    std::string line;
    if (!std::getline(std::cin, line)) return NilValue;
    return objValue(takeString(vm.heap, std::move(line)));
}

Value nativeHimpun(VM& vm, Value, int argc, Value* args) {
    ObjSet* set = newSet(vm.heap);
    vm.heap.protect(objValue(set));
    Value source = argc > 0 ? args[0] : NilValue;
    if (isList(source)) {
        for (Value item : asList(source)->items) set->table.set(item, NilValue);
    } else if (isTuple(source)) {
        for (Value item : asTuple(source)->items) set->table.set(item, NilValue);
    } else if (isSet(source)) {
        for (const Entry& entry : asSet(source)->table.raw()) {
            if (entry.key.bits != EmptyKeyValue.bits && entry.key.bits != TombstoneValue.bits) {
                set->table.set(entry.key, NilValue);
            }
        }
    } else if (isMap(source)) {
        for (const Entry& entry : asMap(source)->table.raw()) {
            if (entry.key.bits != EmptyKeyValue.bits && entry.key.bits != TombstoneValue.bits) {
                set->table.set(entry.key, NilValue);
            }
        }
    } else if (isString(source)) {
        const std::string& chars = asString(source)->chars;
        size_t position = 0;
        while (position < chars.size()) {
            unsigned char lead = static_cast<unsigned char>(chars[position]);
            size_t length = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
            if (position + length > chars.size()) length = chars.size() - position;
            set->table.set(objValue(copyString(vm.heap, std::string_view(chars).substr(position, length))), NilValue);
            position += length;
        }
    } else if (!isNil(source)) {
        vm.heap.unprotect();
        vm.raise("teu bisa dijadikeun himpunan");
        return NilValue;
    }
    vm.heap.unprotect();
    return objValue(set);
}

Value listTambah(VM&, Value receiver, int, Value* args) {
    asList(receiver)->items.push_back(args[0]);
    return receiver;
}

Value listCabut(VM& vm, Value receiver, int, Value*) {
    ObjList* list = asList(receiver);
    if (list->items.empty()) {
        vm.raise("daptar kosong");
        return NilValue;
    }
    Value last = list->items.back();
    list->items.pop_back();
    return last;
}

Value listLeupaskeun(VM& vm, Value receiver, int, Value* args) {
    ObjList* list = asList(receiver);
    if (!isNumber(args[0])) {
        vm.raise("indeks kudu angka");
        return NilValue;
    }
    double position = asNumber(args[0]);
    if (position < 0) position += static_cast<double>(list->items.size());
    if (position < 0 || static_cast<size_t>(position) >= list->items.size()) {
        vm.raise("indeks di luar rentang");
        return NilValue;
    }
    Value removed = list->items[static_cast<size_t>(position)];
    list->items.erase(list->items.begin() + static_cast<size_t>(position));
    return removed;
}

Value listSisipkeun(VM& vm, Value receiver, int, Value* args) {
    ObjList* list = asList(receiver);
    if (!isNumber(args[0])) {
        vm.raise("indeks kudu angka");
        return NilValue;
    }
    double position = asNumber(args[0]);
    if (position < 0) position += static_cast<double>(list->items.size()) + 1;
    if (position < 0 || static_cast<size_t>(position) > list->items.size()) {
        vm.raise("indeks di luar rentang");
        return NilValue;
    }
    list->items.insert(list->items.begin() + static_cast<size_t>(position), args[1]);
    return receiver;
}

Value listIndeks(VM&, Value receiver, int, Value* args) {
    const std::vector<Value>& items = asList(receiver)->items;
    for (size_t index = 0; index < items.size(); ++index) {
        if (valuesEqual(items[index], args[0])) return numberValue(static_cast<double>(index));
    }
    return numberValue(-1);
}

Value listNgandung(VM&, Value receiver, int, Value* args) {
    for (Value item : asList(receiver)->items) {
        if (valuesEqual(item, args[0])) return TrueValue;
    }
    return FalseValue;
}

Value listUrutkeun(VM& vm, Value receiver, int, Value*) {
    ObjList* list = asList(receiver);
    for (Value item : list->items) {
        if (!isNumber(item) && !isString(item)) {
            vm.raise("teu bisa diurutkeun (ngan angka atawa kecap)");
            return NilValue;
        }
    }
    for (size_t index = 1; index < list->items.size(); ++index) {
        if (isNumber(list->items[index]) != isNumber(list->items[0])) {
            vm.raise("teu bisa diurutkeun (jenis campur)");
            return NilValue;
        }
    }
    std::sort(list->items.begin(), list->items.end(), [](Value a, Value b) {
        if (isNumber(a)) return asNumber(a) < asNumber(b);
        return asString(a)->chars < asString(b)->chars;
    });
    return receiver;
}

Value listBalik(VM&, Value receiver, int, Value*) {
    std::reverse(asList(receiver)->items.begin(), asList(receiver)->items.end());
    return receiver;
}

Value listTiron(VM& vm, Value receiver, int, Value*) {
    ObjList* copy = newList(vm.heap);
    copy->items = asList(receiver)->items;
    return objValue(copy);
}

Value listGabung(VM& vm, Value receiver, int argc, Value* args) {
    std::string separator = argc > 0 && isString(args[0]) ? asString(args[0])->chars : "";
    std::string joined;
    const std::vector<Value>& items = asList(receiver)->items;
    for (size_t index = 0; index < items.size(); ++index) {
        if (index > 0) joined += separator;
        joined += isString(items[index]) ? asString(items[index])->chars : valueToString(items[index]);
    }
    return objValue(takeString(vm.heap, std::move(joined)));
}

Value mapKonci(VM& vm, Value receiver, int, Value*) {
    ObjList* list = newList(vm.heap);
    for (const Entry& entry : asMap(receiver)->table.raw()) {
        if (entry.key.bits != EmptyKeyValue.bits && entry.key.bits != TombstoneValue.bits) {
            list->items.push_back(entry.key);
        }
    }
    return objValue(list);
}

Value mapNilai(VM& vm, Value receiver, int, Value*) {
    ObjList* list = newList(vm.heap);
    for (const Entry& entry : asMap(receiver)->table.raw()) {
        if (entry.key.bits != EmptyKeyValue.bits && entry.key.bits != TombstoneValue.bits) {
            list->items.push_back(entry.value);
        }
    }
    return objValue(list);
}

Value mapNgandung(VM&, Value receiver, int, Value* args) {
    Value ignored;
    return asMap(receiver)->table.get(args[0], ignored) ? TrueValue : FalseValue;
}

Value mapCabut(VM&, Value receiver, int, Value* args) {
    ObjMap* map = asMap(receiver);
    Value removed;
    if (!map->table.get(args[0], removed)) return NilValue;
    map->table.remove(args[0]);
    return removed;
}

Value mapTiron(VM& vm, Value receiver, int, Value*) {
    ObjMap* copy = newMap(vm.heap);
    copy->table = asMap(receiver)->table;
    return objValue(copy);
}

Value setTambah(VM&, Value receiver, int, Value* args) {
    asSet(receiver)->table.set(args[0], NilValue);
    return receiver;
}

Value setLeupaskeun(VM&, Value receiver, int, Value* args) {
    return asSet(receiver)->table.remove(args[0]) ? TrueValue : FalseValue;
}

Value setNgandung(VM&, Value receiver, int, Value* args) {
    Value ignored;
    return asSet(receiver)->table.get(args[0], ignored) ? TrueValue : FalseValue;
}

Value setTiron(VM& vm, Value receiver, int, Value*) {
    ObjSet* copy = newSet(vm.heap);
    copy->table = asSet(receiver)->table;
    return objValue(copy);
}

Value stringGede(VM& vm, Value receiver, int, Value*) {
    std::string text = asString(receiver)->chars;
    for (char& c : text) c = std::toupper(static_cast<unsigned char>(c));
    return objValue(takeString(vm.heap, std::move(text)));
}

Value stringLeutik(VM& vm, Value receiver, int, Value*) {
    std::string text = asString(receiver)->chars;
    for (char& c : text) c = std::tolower(static_cast<unsigned char>(c));
    return objValue(takeString(vm.heap, std::move(text)));
}

Value stringBeberesit(VM& vm, Value receiver, int, Value*) {
    const std::string& chars = asString(receiver)->chars;
    size_t begin = 0;
    size_t end = chars.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(chars[begin]))) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(chars[end - 1]))) --end;
    return objValue(copyString(vm.heap, std::string_view(chars).substr(begin, end - begin)));
}

Value stringGanti(VM& vm, Value receiver, int, Value* args) {
    if (!isString(args[0]) || !isString(args[1])) {
        vm.raise("ganti butuh dua kecap");
        return NilValue;
    }
    const std::string& from = asString(args[0])->chars;
    const std::string& to = asString(args[1])->chars;
    std::string result;
    const std::string& text = asString(receiver)->chars;
    if (!from.empty()) {
        size_t position = 0;
        for (;;) {
            size_t hit = text.find(from, position);
            if (hit == std::string::npos) break;
            result += text.substr(position, hit - position);
            result += to;
            position = hit + from.size();
        }
        result += text.substr(position);
    } else {
        result = text;
    }
    return objValue(takeString(vm.heap, std::move(result)));
}

Value stringIndeks(VM&, Value receiver, int, Value* args) {
    if (!isString(args[0])) return numberValue(-1);
    size_t hit = asString(receiver)->chars.find(asString(args[0])->chars);
    if (hit == std::string::npos) return numberValue(-1);
    return numberValue(static_cast<double>(hit));
}

Value stringNgandung(VM&, Value receiver, int, Value* args) {
    if (!isString(args[0])) return FalseValue;
    return asString(receiver)->chars.find(asString(args[0])->chars) != std::string::npos
               ? TrueValue
               : FalseValue;
}

Value stringMeulah(VM& vm, Value receiver, int argc, Value* args) {
    std::string separator = argc > 0 && isString(args[0]) ? asString(args[0])->chars : " ";
    const std::string& text = asString(receiver)->chars;
    ObjList* list = newList(vm.heap);
    vm.heap.protect(objValue(list));
    if (separator.empty()) {
        for (char c : text) {
            list->items.push_back(objValue(copyString(vm.heap, std::string(1, c))));
        }
    } else {
        size_t position = 0;
        for (;;) {
            size_t hit = text.find(separator, position);
            if (hit == std::string::npos) break;
            list->items.push_back(
                objValue(copyString(vm.heap, std::string_view(text).substr(position, hit - position))));
            position = hit + separator.size();
        }
        list->items.push_back(objValue(copyString(vm.heap, std::string_view(text).substr(position))));
    }
    vm.heap.unprotect();
    return objValue(list);
}

Value tupleIndeks(VM&, Value receiver, int, Value* args) {
    const std::vector<Value>& items = asTuple(receiver)->items;
    for (size_t index = 0; index < items.size(); ++index) {
        if (valuesEqual(items[index], args[0])) return numberValue(static_cast<double>(index));
    }
    return numberValue(-1);
}

Value tupleNgandung(VM&, Value receiver, int, Value* args) {
    for (Value item : asTuple(receiver)->items) {
        if (valuesEqual(item, args[0])) return TrueValue;
    }
    return FalseValue;
}

void addMethod(VM& vm, int tableIndex, const char* name, NativeFn function, int arity) {
    ObjString* nameString = copyString(vm.heap, name);
    ObjNative* native = allocateObject<ObjNative>(vm.heap, ObjType::Native);
    native->function = function;
    native->arity = arity;
    native->name = nameString;
    vm.methodTables[tableIndex].set(objValue(nameString), objValue(native));
}

}

void registerBuiltins(VM& vm) {
    struct Builtin {
        const char* name;
        NativeFn function;
        int arity;
    };
    const Builtin builtins[] = {
        {"cetak", nativeCetak, -1},
        {"tulis", nativeTulis, -1},
        {"jenis", nativeJenis, 1},
        {"kecap", nativeKecap, 1},
        {"angka", nativeAngka, 1},
        {"panjang", nativePanjang, 1},
        {"rentang", nativeRentang, -1},
        {"asupan", nativeAsupan, 0},
        {"himpun", nativeHimpun, -1},
    };
    for (const Builtin& builtin : builtins) {
        ObjString* nameString = copyString(vm.heap, builtin.name);
        ObjNative* native = allocateObject<ObjNative>(vm.heap, ObjType::Native);
        native->function = builtin.function;
        native->arity = builtin.arity;
        native->name = nameString;
        vm.builtins.set(objValue(nameString), objValue(native));
    }

    addMethod(vm, 0, "tambah", listTambah, 1);
    addMethod(vm, 0, "cabut", listCabut, 0);
    addMethod(vm, 0, "leupaskeun", listLeupaskeun, 1);
    addMethod(vm, 0, "sisipkeun", listSisipkeun, 2);
    addMethod(vm, 0, "indeks", listIndeks, 1);
    addMethod(vm, 0, "ngandung", listNgandung, 1);
    addMethod(vm, 0, "urutkeun", listUrutkeun, 0);
    addMethod(vm, 0, "balik", listBalik, 0);
    addMethod(vm, 0, "tiron", listTiron, 0);
    addMethod(vm, 0, "gabung", listGabung, -1);

    addMethod(vm, 1, "konci", mapKonci, 0);
    addMethod(vm, 1, "nilai", mapNilai, 0);
    addMethod(vm, 1, "ngandung", mapNgandung, 1);
    addMethod(vm, 1, "cabut", mapCabut, 1);
    addMethod(vm, 1, "tiron", mapTiron, 0);

    addMethod(vm, 2, "tambah", setTambah, 1);
    addMethod(vm, 2, "leupaskeun", setLeupaskeun, 1);
    addMethod(vm, 2, "ngandung", setNgandung, 1);
    addMethod(vm, 2, "tiron", setTiron, 0);

    addMethod(vm, 3, "gede", stringGede, 0);
    addMethod(vm, 3, "leutik", stringLeutik, 0);
    addMethod(vm, 3, "beberesit", stringBeberesit, 0);
    addMethod(vm, 3, "ganti", stringGanti, 2);
    addMethod(vm, 3, "indeks", stringIndeks, 1);
    addMethod(vm, 3, "ngandung", stringNgandung, 1);
    addMethod(vm, 3, "meulah", stringMeulah, -1);

    addMethod(vm, 4, "indeks", tupleIndeks, 1);
    addMethod(vm, 4, "ngandung", tupleNgandung, 1);
}

}
