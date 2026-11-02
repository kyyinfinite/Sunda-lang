#include "modules.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <thread>

#ifdef _WIN32
#include <direct.h>
#define SUNDA_GETCWD _getcwd
#else
#include <unistd.h>
#define SUNDA_GETCWD getcwd
#endif

#include "memory.h"
#include "object.h"
#include "vm.h"

namespace sunda {

namespace {

std::mt19937& rng() {
    static std::mt19937 generator(std::random_device{}());
    return generator;
}

void putNative(VM& vm, ObjModule* module, const char* name, NativeFn function, int arity) {
    ObjString* nameString = copyString(vm.heap, name);
    ObjNative* native = allocateObject<ObjNative>(vm.heap, ObjType::Native);
    native->function = function;
    native->arity = arity;
    native->name = nameString;
    module->globals.set(objValue(nameString), objValue(native));
}

Value mathAkar(VM& vm, Value, int, Value* args) {
    if (!isNumber(args[0])) {
        vm.raise("akar butuh angka");
        return NilValue;
    }
    return numberValue(std::sqrt(asNumber(args[0])));
}

Value mathSin(VM& vm, Value, int, Value* args) {
    if (!isNumber(args[0])) {
        vm.raise("butuh angka");
        return NilValue;
    }
    return numberValue(std::sin(asNumber(args[0])));
}

Value mathCos(VM& vm, Value, int, Value* args) {
    if (!isNumber(args[0])) {
        vm.raise("butuh angka");
        return NilValue;
    }
    return numberValue(std::cos(asNumber(args[0])));
}

Value mathTan(VM& vm, Value, int, Value* args) {
    if (!isNumber(args[0])) {
        vm.raise("butuh angka");
        return NilValue;
    }
    return numberValue(std::tan(asNumber(args[0])));
}

Value mathLog(VM& vm, Value, int, Value* args) {
    if (!isNumber(args[0])) {
        vm.raise("butuh angka");
        return NilValue;
    }
    return numberValue(std::log(asNumber(args[0])));
}

Value mathLantai(VM& vm, Value, int, Value* args) {
    if (!isNumber(args[0])) {
        vm.raise("butuh angka");
        return NilValue;
    }
    return numberValue(std::floor(asNumber(args[0])));
}

Value mathLangit(VM& vm, Value, int, Value* args) {
    if (!isNumber(args[0])) {
        vm.raise("butuh angka");
        return NilValue;
    }
    return numberValue(std::ceil(asNumber(args[0])));
}

Value mathAbsolute(VM& vm, Value, int, Value* args) {
    if (!isNumber(args[0])) {
        vm.raise("butuh angka");
        return NilValue;
    }
    return numberValue(std::fabs(asNumber(args[0])));
}

Value mathBulat(VM& vm, Value, int, Value* args) {
    if (!isNumber(args[0])) {
        vm.raise("butuh angka");
        return NilValue;
    }
    return numberValue(std::round(asNumber(args[0])));
}

Value mathMinimal(VM& vm, Value, int, Value* args) {
    if (!isNumber(args[0]) || !isNumber(args[1])) {
        vm.raise("butuh angka");
        return NilValue;
    }
    return numberValue(std::min(asNumber(args[0]), asNumber(args[1])));
}

Value mathMaksimal(VM& vm, Value, int, Value* args) {
    if (!isNumber(args[0]) || !isNumber(args[1])) {
        vm.raise("butuh angka");
        return NilValue;
    }
    return numberValue(std::max(asNumber(args[0]), asNumber(args[1])));
}

Value randomAcak(VM&, Value, int argc, Value* args) {
    if (argc == 0) {
        std::uniform_real_distribution<double> distribution(0.0, 1.0);
        return numberValue(distribution(rng()));
    }
    if (argc == 2 && isNumber(args[0]) && isNumber(args[1])) {
        long long a = static_cast<long long>(asNumber(args[0]));
        long long b = static_cast<long long>(asNumber(args[1]));
        if (a > b) std::swap(a, b);
        std::uniform_int_distribution<long long> distribution(a, b);
        return numberValue(static_cast<double>(distribution(rng())));
    }
    return numberValue(static_cast<double>(rng()() % 1000000) / 1000000.0);
}

Value randomPilih(VM& vm, Value, int, Value* args) {
    if (!isList(args[0]) || asList(args[0])->items.empty()) {
        vm.raise("pilih butuh daptar nu teu kosong");
        return NilValue;
    }
    const std::vector<Value>& items = asList(args[0])->items;
    std::uniform_int_distribution<size_t> distribution(0, items.size() - 1);
    return items[distribution(rng())];
}

Value randomSiki(VM&, Value, int, Value* args) {
    unsigned seed = isNumber(args[0]) ? static_cast<unsigned>(asNumber(args[0])) : 0;
    rng().seed(seed);
    return NilValue;
}

Value waktuAyeuna(VM&, Value, int, Value*) {
    double now = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch())
                     .count();
    return numberValue(now);
}

Value waktuNitih(VM&, Value, int, Value*) {
    double now =
        std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    return numberValue(now);
}

Value waktuSare(VM& vm, Value, int, Value* args) {
    if (!isNumber(args[0])) {
        vm.raise("sare butuh angka");
        return NilValue;
    }
    double seconds = asNumber(args[0]);
    if (seconds > 0) std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
    return NilValue;
}

Value fileBaca(VM& vm, Value, int, Value* args) {
    if (!isString(args[0])) {
        vm.raise("baca butuh ngaran berkas");
        return NilValue;
    }
    std::FILE* file = std::fopen(asString(args[0])->chars.c_str(), "rb");
    if (file == nullptr) {
        vm.raise("berkas teu kapanggih: " + asString(args[0])->chars);
        return NilValue;
    }
    std::string contents;
    char buffer[4096];
    size_t read = 0;
    while ((read = std::fread(buffer, 1, sizeof(buffer), file)) > 0) contents.append(buffer, read);
    std::fclose(file);
    return objValue(takeString(vm.heap, std::move(contents)));
}

Value fileTulis(VM& vm, Value, int, Value* args) {
    if (!isString(args[0])) {
        vm.raise("tulis butuh ngaran berkas");
        return NilValue;
    }
    std::string text = valueToString(args[1]);
    std::FILE* file = std::fopen(asString(args[0])->chars.c_str(), "wb");
    if (file == nullptr) {
        vm.raise("teu bisa nulis berkas: " + asString(args[0])->chars);
        return NilValue;
    }
    std::fwrite(text.data(), 1, text.size(), file);
    std::fclose(file);
    return TrueValue;
}

Value fileTambahan(VM& vm, Value, int, Value* args) {
    if (!isString(args[0])) {
        vm.raise("tambahan butuh ngaran berkas");
        return NilValue;
    }
    std::string text = valueToString(args[1]);
    std::FILE* file = std::fopen(asString(args[0])->chars.c_str(), "ab");
    if (file == nullptr) {
        vm.raise("teu bisa nambihan berkas: " + asString(args[0])->chars);
        return NilValue;
    }
    std::fwrite(text.data(), 1, text.size(), file);
    std::fclose(file);
    return TrueValue;
}

Value fileAya(VM&, Value, int, Value* args) {
    if (!isString(args[0])) return FalseValue;
    std::FILE* file = std::fopen(asString(args[0])->chars.c_str(), "rb");
    if (file == nullptr) return FalseValue;
    std::fclose(file);
    return TrueValue;
}

Value fileHapus(VM&, Value, int, Value* args) {
    if (!isString(args[0])) return FalseValue;
    return std::remove(asString(args[0])->chars.c_str()) == 0 ? TrueValue : FalseValue;
}

Value sistemArgumen(VM& vm, Value, int, Value*) {
    ObjList* list = newList(vm.heap);
    vm.heap.protect(objValue(list));
    for (const std::string& argument : vm.scriptArgs) {
        list->items.push_back(objValue(copyString(vm.heap, argument)));
    }
    vm.heap.unprotect();
    return objValue(list);
}

Value sistemDirektori(VM& vm, Value, int, Value*) {
    std::string buffer;
    buffer.resize(4096);
    if (SUNDA_GETCWD(buffer.data(), buffer.size()) == nullptr) return NilValue;
    buffer.resize(std::strlen(buffer.c_str()));
    return objValue(takeString(vm.heap, std::move(buffer)));
}

}

bool nativeModule(VM& vm, const std::string& name, Value& out) {
    ObjString* moduleName = copyString(vm.heap, name);
    ObjModule* module = newModule(vm.heap, moduleName);
    if (name == "matematika") {
        module->globals.set(objValue(copyString(vm.heap, "pi")), numberValue(3.141592653589793));
        module->globals.set(objValue(copyString(vm.heap, "e")), numberValue(2.718281828459045));
        putNative(vm, module, "akar", mathAkar, 1);
        putNative(vm, module, "sin", mathSin, 1);
        putNative(vm, module, "cos", mathCos, 1);
        putNative(vm, module, "tan", mathTan, 1);
        putNative(vm, module, "log", mathLog, 1);
        putNative(vm, module, "lantai", mathLantai, 1);
        putNative(vm, module, "langit", mathLangit, 1);
        putNative(vm, module, "absolute", mathAbsolute, 1);
        putNative(vm, module, "bulat", mathBulat, 1);
        putNative(vm, module, "minimal", mathMinimal, 2);
        putNative(vm, module, "maksimal", mathMaksimal, 2);
    } else if (name == "random") {
        putNative(vm, module, "acak", randomAcak, -1);
        putNative(vm, module, "pilih", randomPilih, 1);
        putNative(vm, module, "siki", randomSiki, 1);
    } else if (name == "waktu") {
        putNative(vm, module, "ayeuna", waktuAyeuna, 0);
        putNative(vm, module, "nitih", waktuNitih, 0);
        putNative(vm, module, "sare", waktuSare, 1);
    } else if (name == "file") {
        putNative(vm, module, "baca", fileBaca, 1);
        putNative(vm, module, "tulis", fileTulis, 2);
        putNative(vm, module, "tambahan", fileTambahan, 2);
        putNative(vm, module, "aya", fileAya, 1);
        putNative(vm, module, "hapus", fileHapus, 1);
    } else if (name == "sistem") {
        putNative(vm, module, "argumen", sistemArgumen, 0);
        putNative(vm, module, "direktori", sistemDirektori, 0);
    } else {
        return false;
    }
    out = objValue(module);
    return true;
}

}
