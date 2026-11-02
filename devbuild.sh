#!/bin/sh
set -e
cd "$(dirname "$0")"
mkdir -p build
FLAGS="-std=c++20 -O1 -Wall -Wextra -Isrc"
if [ "$1" = "sanitize" ]; then
    FLAGS="$FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer -g"
fi
SRCS="src/lexer.cpp src/parser.cpp src/resolver.cpp src/compiler.cpp src/chunk.cpp src/object.cpp src/table.cpp src/memory.cpp src/vm.cpp src/native.cpp src/modules.cpp src/formatter.cpp src/cli.cpp src/main.cpp"
g++ $FLAGS $SRCS -o build/sunda
