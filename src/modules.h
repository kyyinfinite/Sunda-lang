#pragma once

#include <string>

#include "value.h"

namespace sunda {

struct VM;

bool nativeModule(VM& vm, const std::string& name, Value& out);

}
