#include <string>
#include <vector>

#include "cli.h"

int main(int argc, char** argv) {
    std::vector<std::string> arguments(argv + 1, argv + argc);
    std::string self = argc > 0 ? argv[0] : "sunda";
    return sunda::runCli(arguments, self);
}
