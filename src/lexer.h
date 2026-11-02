#pragma once

#include <string_view>
#include <vector>

#include "token.h"

namespace sunda {

struct FormatToken {
    TokenType type = TokenType::Eof;
    std::string_view lexeme;
    int line = 1;
    int column = 1;
    bool comment = false;
};

class Lexer {
public:
    explicit Lexer(std::string_view source);
    std::vector<Token> tokenize();
    std::vector<FormatToken> tokenizeForFormat();

private:
    bool atEnd() const { return current >= source.size(); }
    char peek() const { return atEnd() ? '\0' : source[current]; }
    char peekNext() const { return current + 1 >= source.size() ? '\0' : source[current + 1]; }
    char advance() { return source[current++]; }
    bool match(char expected);
    int columnOf(size_t offset) const { return static_cast<int>(offset - lineStart) + 1; }
    bool needsNewline() const;
    void addToken(TokenType type, double number = 0);
    void scanToken();
    void scanString();
    void scanNumber();
    void scanIdentifier();

    std::string_view source;
    std::vector<Token> tokens;
    size_t start = 0;
    size_t current = 0;
    size_t lineStart = 0;
    int line = 1;
    int bracketDepth = 0;
};

}
