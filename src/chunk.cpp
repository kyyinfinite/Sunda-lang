#include "chunk.h"

namespace sunda {

void Chunk::write(uint8_t byte, int line) {
    code.push_back(byte);
    lines.push_back(line);
}

size_t Chunk::addConstant(Value value) {
    constants.push_back(value);
    return constants.size() - 1;
}

}
