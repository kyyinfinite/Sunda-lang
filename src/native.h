#pragma once

#include "value.h"

namespace sunda {

struct VM;

using NativeFn = Value (*)(VM& vm, Value receiver, int argc, Value* args);

void registerBuiltins(VM& vm);

}
