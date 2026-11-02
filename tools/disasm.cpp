#include <cstdio>
#include <string>
#include <vector>

#include "compiler.h"
#include "lexer.h"
#include "parser.h"
#include "resolver.h"
#include "vm.h"

using namespace sunda;

namespace {

const char* opName(uint8_t op) {
    static const char* names[] = {
#define X(name) #name,
        SUNDA_OPCODES(X)
#undef X
    };
    return op < sizeof(names) / sizeof(names[0]) ? names[op] : "?";
}

void disassemble(const Chunk& chunk, const std::string& indent) {
    size_t offset = 0;
    while (offset < chunk.code.size()) {
        int line = offset < chunk.lines.size() ? chunk.lines[offset] : 0;
        std::printf("%s%04zu %-14s", indent.c_str(), offset, opName(chunk.code[offset]));
        OpCode op = static_cast<OpCode>(chunk.code[offset]);
        size_t width = 1;
        switch (op) {
            case OpCode::Constant:
            case OpCode::GetGlobal:
            case OpCode::SetGlobal:
            case OpCode::GetLocal:
            case OpCode::SetLocal:
            case OpCode::GetUpvalue:
            case OpCode::SetUpvalue:
            case OpCode::GetProperty:
            case OpCode::SetProperty:
            case OpCode::List:
            case OpCode::Map:
            case OpCode::Tuple:
            case OpCode::Set:
            case OpCode::Closure:
            case OpCode::CloseUpvalueFrom: {
                uint16_t operand = (chunk.code[offset + 1] << 8) | chunk.code[offset + 2];
                std::printf("%d", operand);
                if (op == OpCode::Constant && offset + 2 < chunk.code.size()) {
                    Value v = chunk.constants[operand];
                    std::printf("  ; %s", valueToString(v).c_str());
                }
                if (op == OpCode::Closure) {
                    uint8_t count = chunk.code[offset + 3];
                    std::printf(" upvalues=%d", count);
                    width = 3 + 1 + count * 2;
                } else {
                    width = 3;
                }
                break;
            }
            case OpCode::Jump:
            case OpCode::JumpIfFalse:
            case OpCode::JumpIfTrue:
            case OpCode::Loop:
            case OpCode::Try: {
                uint16_t operand = (chunk.code[offset + 1] << 8) | chunk.code[offset + 2];
                std::printf("-> %d", static_cast<int>(offset) + 3 + operand);
                width = 3;
                break;
            }
            case OpCode::IterNext: {
                uint16_t slot = (chunk.code[offset + 1] << 8) | chunk.code[offset + 2];
                uint16_t operand = (chunk.code[offset + 3] << 8) | chunk.code[offset + 4];
                std::printf("slot=%d -> %d", slot, static_cast<int>(offset) + 5 + operand);
                width = 5;
                break;
            }
            case OpCode::Import: {
                uint16_t name = (chunk.code[offset + 1] << 8) | chunk.code[offset + 2];
                std::printf("name=%d kind=%d slot=%d", name, chunk.code[offset + 3],
                            (chunk.code[offset + 4] << 8) | chunk.code[offset + 5]);
                width = 6;
                break;
            }
            case OpCode::GetSuper:
            case OpCode::Invoke:
            case OpCode::DefineClass: {
                uint16_t a = (chunk.code[offset + 1] << 8) | chunk.code[offset + 2];
                std::printf("%d argc/num=%d", a, chunk.code[offset + 3]);
                width = 4;
                break;
            }
            case OpCode::PopN:
            case OpCode::CloseUpvalueN:
            case OpCode::Slice:
            case OpCode::Call:
                std::printf("%d", chunk.code[offset + 1]);
                width = 2;
                break;
            default:
                break;
        }
        std::printf("   (baris %d)\n", line);
        offset += width;
    }
}

}

int main(int argc, char** argv) {
    if (argc != 2) return 64;
    std::FILE* file = std::fopen(argv[1], "rb");
    if (file == nullptr) return 66;
    std::string source;
    char buffer[4096];
    size_t read = 0;
    while ((read = std::fread(buffer, 1, sizeof(buffer), file)) > 0) source.append(buffer, read);
    std::fclose(file);

    VM vm;
    vm.heap.pauseGC();
    Lexer lexer(source);
    Parser parser(lexer.tokenize());
    Program program = parser.parse();
    Resolver resolver;
    resolver.resolve(program);
    Compiler compiler(vm.heap);
    ObjFunction* function = compiler.compile(program, source, "<utama>");
    vm.heap.resumeGC();
    disassemble(function->chunk, "");
    std::printf("--- konstanta:\n");
    for (size_t index = 0; index < function->chunk.constants.size(); ++index) {
        Value value = function->chunk.constants[index];
        if (isObj(value) && asObj(value)->type == ObjType::Function) {
            ObjFunction* inner = static_cast<ObjFunction*>(asObj(value));
            std::printf("[%zu] pagawean %s (arity %d-%d, slots %d)\n", index,
                        inner->name->chars.c_str(), inner->arityMin, inner->arityMax,
                        inner->slotCount);
            disassemble(inner->chunk, "    ");
        } else {
            std::printf("[%zu] %s\n", index, valueToString(value).c_str());
        }
    }
    return 0;
}
