#include "cli.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "formatter.h"
#include "vm.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace sunda {

namespace {

const char* UsageText =
    "Cara ngagunakeun: sunda <parentah> [argumen]\n"
    "\n"
    "Parentah:\n"
    "  run <berkas.sd> [argumen]  ngajalankeun program\n"
    "  repl                       sesi interaktif\n"
    "  test [tunjuk...]           ngajalankeun tés (standar: tests/)\n"
    "  check <berkas.sd>          pariksa sintaksis tanpa ngajalankeun\n"
    "  format <berkas.sd>         ngabéréskeun sumber ka stdout\n"
    "  help                       nuduhkeun pitulung ieu\n";

std::string selfPath;

std::string executablePath() {
#ifdef _WIN32
    char buffer[MAX_PATH];
    GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    return std::string(buffer);
#else
    char buffer[4096];
    ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (length <= 0) return selfPath;
    buffer[length] = '\0';
    return std::string(buffer);
#endif
}

bool readFile(const std::string& path, std::string& out) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    std::stringstream buffer;
    buffer << input.rdbuf();
    out = buffer.str();
    return true;
}

int runFile(const std::string& path, const std::vector<std::string>& arguments) {
    std::string source;
    if (!readFile(path, source)) {
        std::fprintf(stderr, "Berkas teu kapanggih: %s\n", path.c_str());
        return 66;
    }
    VM vm;
    vm.scriptArgs.assign(arguments.begin(), arguments.end());
    switch (vm.interpret(source, path)) {
        case InterpretResult::Ok:
            return 0;
        case InterpretResult::CompileError:
            return 65;
        case InterpretResult::RuntimeError:
            return 70;
    }
    return 70;
}

int runRepl() {
    VM vm;
    vm.replMode = true;
    std::fprintf(stdout, "Sunda REPL — tulis kode, Ctrl-D pikeun kaluar\n");
    std::string pending;
    while (true) {
        if (pending.empty()) std::fputs(">>> ", stdout);
        else std::fputs("... ", stdout);
        std::fflush(stdout);
        std::string line;
        if (!std::getline(std::cin, line)) break;

        std::string candidate = pending.empty() ? line : pending + "\n" + line;
        int braces = 0;
        bool inString = false;
        for (size_t index = 0; index < candidate.size(); ++index) {
            char c = candidate[index];
            if (inString) {
                if (c == '\\') ++index;
                else if (c == '"') inString = false;
                continue;
            }
            if (c == '#') {
                while (index < candidate.size() && candidate[index] != '\n') ++index;
                continue;
            }
            if (c == '"') inString = true;
            else if (c == '{' || c == '(' || c == '[') ++braces;
            else if (c == '}' || c == ')' || c == ']') --braces;
        }
        if (braces > 0 || inString) {
            pending = candidate;
            continue;
        }
        pending.clear();
        if (candidate.find_first_not_of(" \t\r\n") == std::string::npos) continue;
        switch (vm.interpret(candidate)) {
            case InterpretResult::Ok:
                break;
            case InterpretResult::CompileError:
                break;
            case InterpretResult::RuntimeError:
                break;
        }
    }
    std::fputs("\n", stdout);
    return 0;
}

bool hasExpectation(const std::string& sourcePath, std::string& expected, int& expectedExit) {
    std::string base = sourcePath.substr(0, sourcePath.size() - 3);
    expectedExit = 0;
    std::string exitPath = base + ".exit";
    std::string exitContent;
    if (readFile(exitPath, exitContent)) {
        try {
            expectedExit = std::stoi(exitContent);
        } catch (...) {
            expectedExit = 0;
        }
    }
    return readFile(base + ".expected", expected);
}

int runTests(const std::vector<std::string>& paths) {
    std::vector<std::string> files;
    for (const std::string& target : paths.empty() ? std::vector<std::string>{"tests"} : paths) {
        if (target.size() > 3 && target.substr(target.size() - 3) == ".sd") {
            files.push_back(target);
            continue;
        }
        std::string command = "for f in " + target + "/*.sd; do echo \"$f\"; done";
        FILE* pipe = popen(command.c_str(), "r");
        if (pipe == nullptr) {
            std::fprintf(stderr, "Teu bisa maca diréktori: %s\n", target.c_str());
            return 2;
        }
        char buffer[1024];
        while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string line(buffer);
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
            if (!line.empty()) files.push_back(line);
        }
        pclose(pipe);
    }
    std::string self = executablePath();
    int passed = 0;
    int failed = 0;
    for (const std::string& file : files) {
        std::string expected;
        int expectedExit = 0;
        if (!hasExpectation(file, expected, expectedExit)) continue;
        std::string temporary = "/tmp/sunda_test_outputXXXXXX";
        std::vector<char> tempBuffer(temporary.begin(), temporary.end());
        tempBuffer.push_back('\0');
        int descriptor = mkstemp(tempBuffer.data());
        if (descriptor < 0) {
            std::fprintf(stderr, "Teu bisa nyieun berkas samentara\n");
            return 2;
        }
        close(descriptor);
        std::string command = "\"" + self + "\" run \"" + file + "\" > \"" +
                              tempBuffer.data() + "\" 2>/dev/null";
        int status = std::system(command.c_str());
        int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        std::string output;
        readFile(tempBuffer.data(), output);
        std::remove(tempBuffer.data());
        bool stdoutOk = output == expected;
        bool exitOk = exitCode == expectedExit;
        if (stdoutOk && exitOk) {
            ++passed;
        } else {
            ++failed;
            std::printf("GAGAL: %s\n", file.c_str());
            if (!stdoutOk) {
                std::printf("  stdout: diarepkeun %d byte, narima %d byte\n",
                            static_cast<int>(expected.size()), static_cast<int>(output.size()));
            }
            if (!exitOk) {
                std::printf("  kode kaluar: diarepkeun %d, narima %d\n", expectedExit, exitCode);
            }
        }
    }
    std::printf("lulus %d, gagal %d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}

int runCheck(const std::string& path) {
    std::string source;
    if (!readFile(path, source)) {
        std::fprintf(stderr, "Berkas teu kapanggih: %s\n", path.c_str());
        return 66;
    }
    VM vm;
    return vm.compileCheck(source) ? 0 : 65;
}

int runFormat(const std::string& path) {
    std::string source;
    if (!readFile(path, source)) {
        std::fprintf(stderr, "Berkas teu kapanggih: %s\n", path.c_str());
        return 66;
    }
    try {
        std::string formatted = formatSource(source);
        std::fwrite(formatted.data(), 1, formatted.size(), stdout);
        return 0;
    } catch (const SyntaxError& error) {
        std::fprintf(stderr, "Aya nu salah di baris %d: %s\n", error.line, error.message.c_str());
        return 65;
    }
}

}

int runCli(const std::vector<std::string>& arguments, const std::string& self) {
    selfPath = self;
    if (arguments.empty()) {
        std::fputs(UsageText, stderr);
        return 64;
    }
    const std::string& command = arguments[0];
    if (command == "help" || command == "--help" || command == "-h") {
        std::fputs(UsageText, stdout);
        return 0;
    }
    if (command == "run") {
        if (arguments.size() < 2) {
            std::fputs("Cara ngagunakeun: sunda run <berkas.sd> [argumen]\n", stderr);
            return 64;
        }
        std::vector<std::string> scriptArguments(arguments.begin() + 2, arguments.end());
        return runFile(arguments[1], scriptArguments);
    }
    if (command == "repl") return runRepl();
    if (command == "test") {
        std::vector<std::string> targets(arguments.begin() + 1, arguments.end());
        return runTests(targets);
    }
    if (command == "check") {
        if (arguments.size() != 2) {
            std::fputs("Cara ngagunakeun: sunda check <berkas.sd>\n", stderr);
            return 64;
        }
        return runCheck(arguments[1]);
    }
    if (command == "format") {
        if (arguments.size() != 2) {
            std::fputs("Cara ngagunakeun: sunda format <berkas.sd>\n", stderr);
            return 64;
        }
        return runFormat(arguments[1]);
    }
    std::fprintf(stderr, "Parentah teu dikenal: %s\n", command.c_str());
    std::fputs(UsageText, stderr);
    return 64;
}

}
