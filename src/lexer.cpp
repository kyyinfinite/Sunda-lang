#include "lexer.h"

#include <cstdlib>
#include <string>

namespace sunda {

namespace {

struct Keyword {
    std::string_view text;
    TokenType type;
};

constexpr Keyword Keywords[] = {
    {"lamun", TokenType::Lamun},
    {"lamunteu", TokenType::Lamunteu},
    {"salila", TokenType::Salila},
    {"pikeun", TokenType::Pikeun},
    {"dina", TokenType::Dina},
    {"pagawean", TokenType::Pagawean},
    {"balikeun", TokenType::Balikeun},
    {"eureun", TokenType::Eureun},
    {"teruskeun", TokenType::Teruskeun},
    {"kelas", TokenType::Kelas},
    {"turunan", TokenType::Turunan},
    {"ieu", TokenType::Ieu},
    {"induk", TokenType::Induk},
    {"awal", TokenType::Awal},
    {"bener", TokenType::Bener},
    {"salah", TokenType::Salah},
    {"kosong", TokenType::Kosong},
    {"jeung", TokenType::Jeung},
    {"atawa", TokenType::Atawa},
    {"lain", TokenType::Lain},
    {"impor", TokenType::Impor},
    {"cobaan", TokenType::Cobaan},
    {"gagal", TokenType::Gagal},
};

bool isDigit(char c) { return c >= '0' && c <= '9'; }

bool isIdentifierStart(char c) {
    unsigned char byte = static_cast<unsigned char>(c);
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || byte >= 0x80;
}

bool isIdentifierPart(char c) { return isIdentifierStart(c) || isDigit(c); }

bool endsStatement(TokenType type) {
    switch (type) {
        case TokenType::Identifier:
        case TokenType::Number:
        case TokenType::String:
        case TokenType::Bener:
        case TokenType::Salah:
        case TokenType::Kosong:
        case TokenType::Ieu:
        case TokenType::RightParen:
        case TokenType::RightBracket:
        case TokenType::RightBrace:
        case TokenType::Balikeun:
        case TokenType::Eureun:
        case TokenType::Teruskeun:
            return true;
        default:
            return false;
    }
}

TokenType keywordType(std::string_view text) {
    for (const Keyword& keyword : Keywords) {
        if (keyword.text == text) return keyword.type;
    }
    return TokenType::Identifier;
}

}

Lexer::Lexer(std::string_view text) : source(text) {}

bool Lexer::match(char expected) {
    if (atEnd() || source[current] != expected) return false;
    ++current;
    return true;
}

bool Lexer::needsNewline() const {
    return !tokens.empty() && endsStatement(tokens.back().type);
}

void Lexer::addToken(TokenType type, double number) {
    tokens.push_back(Token{type, source.substr(start, current - start), line, columnOf(start), number});
}

std::vector<Token> Lexer::tokenize() {
    while (!atEnd()) scanToken();
    start = current;
    if (needsNewline()) addToken(TokenType::Newline);
    addToken(TokenType::Eof);
    return std::move(tokens);
}

std::vector<FormatToken> Lexer::tokenizeForFormat() {
    std::vector<FormatToken> tokens;
    lineStart = 0;
    line = 1;
    current = 0;
    while (!atEnd()) {
        start = current;
        char c = advance();
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
                break;
            case '\n':
                tokens.push_back(FormatToken{TokenType::Newline, source.substr(start, current - start), line, columnOf(start)});
                ++line;
                lineStart = current;
                break;
            case '#': {
                while (!atEnd() && peek() != '\n') advance();
                tokens.push_back(FormatToken{TokenType::Identifier, source.substr(start, current - start), line, columnOf(start), true});
                break;
            }
            default: {
                if (isDigit(c)) {
                    while (isDigit(peek())) advance();
                    if (peek() == '.' && isDigit(peekNext())) {
                        advance();
                        while (isDigit(peek())) advance();
                    }
                    tokens.push_back(FormatToken{TokenType::Number, source.substr(start, current - start), line, columnOf(start)});
                    break;
                }
                if (isIdentifierStart(c)) {
                    while (isIdentifierPart(peek())) advance();
                    tokens.push_back(FormatToken{keywordType(source.substr(start, current - start)), source.substr(start, current - start), line, columnOf(start)});
                    break;
                }
                if (c == '"') {
                    while (!atEnd() && peek() != '"' && peek() != '\n') {
                        if (peek() == '\\' && peekNext() != '\n' && peekNext() != '\0') advance();
                        advance();
                    }
                    if (!atEnd() && peek() == '"') advance();
                    tokens.push_back(FormatToken{TokenType::String, source.substr(start, current - start), line, columnOf(start)});
                    break;
                }
                if (c == '.' && peek() == '.' && peekNext() == '.') {
                    advance();
                    advance();
                    tokens.push_back(FormatToken{TokenType::Ellipsis, source.substr(start, current - start), line, columnOf(start)});
                    break;
                }
                TokenType type = TokenType::Identifier;
                switch (c) {
                    case '(': type = TokenType::LeftParen; break;
                    case ')': type = TokenType::RightParen; break;
                    case '{': type = TokenType::LeftBrace; break;
                    case '}': type = TokenType::RightBrace; break;
                    case '[': type = TokenType::LeftBracket; break;
                    case ']': type = TokenType::RightBracket; break;
                    case ',': type = TokenType::Comma; break;
                    case '.': type = TokenType::Dot; break;
                    case ':': type = TokenType::Colon; break;
                    case '+': type = match('=') ? TokenType::PlusEqual : TokenType::Plus; break;
                    case '-': type = match('=') ? TokenType::MinusEqual : TokenType::Minus; break;
                    case '/': type = match('=') ? TokenType::SlashEqual : TokenType::Slash; break;
                    case '%': type = TokenType::Percent; break;
                    case '*': type = match('*') ? TokenType::StarStar : (match('=') ? TokenType::StarEqual : TokenType::Star); break;
                    case '=': type = match('=') ? TokenType::EqualEqual : TokenType::Equal; break;
                    case '<': type = match('=') ? TokenType::LessEqual : TokenType::Less; break;
                    case '>': type = match('=') ? TokenType::GreaterEqual : TokenType::Greater; break;
                    case '!': type = match('=') ? TokenType::BangEqual : TokenType::Identifier; break;
                    default: break;
                }
                tokens.push_back(FormatToken{type, source.substr(start, current - start), line, columnOf(start)});
                break;
            }
        }
    }
    return tokens;
}

void Lexer::scanToken() {
    start = current;
    char c = advance();
    switch (c) {
        case ' ':
        case '\t':
        case '\r':
            return;
        case '\n':
            if (bracketDepth == 0 && needsNewline()) addToken(TokenType::Newline);
            ++line;
            lineStart = current;
            return;
        case '#':
            while (!atEnd() && peek() != '\n') advance();
            return;
        case '(': ++bracketDepth; addToken(TokenType::LeftParen); return;
        case ')': if (bracketDepth > 0) --bracketDepth; addToken(TokenType::RightParen); return;
        case '[': ++bracketDepth; addToken(TokenType::LeftBracket); return;
        case ']': if (bracketDepth > 0) --bracketDepth; addToken(TokenType::RightBracket); return;
        case '{': addToken(TokenType::LeftBrace); return;
        case '}': addToken(TokenType::RightBrace); return;
        case ',': addToken(TokenType::Comma); return;
        case ':': addToken(TokenType::Colon); return;
        case '%': addToken(TokenType::Percent); return;
        case '+': addToken(match('=') ? TokenType::PlusEqual : TokenType::Plus); return;
        case '-': addToken(match('=') ? TokenType::MinusEqual : TokenType::Minus); return;
        case '/': addToken(match('=') ? TokenType::SlashEqual : TokenType::Slash); return;
        case '*':
            if (match('*')) addToken(TokenType::StarStar);
            else addToken(match('=') ? TokenType::StarEqual : TokenType::Star);
            return;
        case '=': addToken(match('=') ? TokenType::EqualEqual : TokenType::Equal); return;
        case '<': addToken(match('=') ? TokenType::LessEqual : TokenType::Less); return;
        case '>': addToken(match('=') ? TokenType::GreaterEqual : TokenType::Greater); return;
        case '.':
            if (peek() == '.' && peekNext() == '.') {
                advance();
                advance();
                addToken(TokenType::Ellipsis);
                return;
            }
            addToken(TokenType::Dot);
            return;
        case '!':
            if (match('=')) {
                addToken(TokenType::BangEqual);
                return;
            }
            throw SyntaxError{line, columnOf(start), "karakter teu dikenal"};
        case '"': scanString(); return;
        default:
            if (isDigit(c)) {
                scanNumber();
                return;
            }
            if (isIdentifierStart(c)) {
                scanIdentifier();
                return;
            }
            throw SyntaxError{line, columnOf(start), "karakter teu dikenal"};
    }
}

void Lexer::scanString() {
    while (!atEnd() && peek() != '"' && peek() != '\n') {
        if (peek() == '\\' && peekNext() != '\n' && peekNext() != '\0') advance();
        advance();
    }
    if (atEnd() || peek() == '\n') throw SyntaxError{line, columnOf(start), "kecap teu ditutup"};
    advance();
    addToken(TokenType::String);
}

void Lexer::scanNumber() {
    while (isDigit(peek())) advance();
    if (peek() == '.' && isDigit(peekNext())) {
        advance();
        while (isDigit(peek())) advance();
    }
    std::string text(source.substr(start, current - start));
    addToken(TokenType::Number, std::strtod(text.c_str(), nullptr));
}

void Lexer::scanIdentifier() {
    while (isIdentifierPart(peek())) advance();
    addToken(keywordType(source.substr(start, current - start)));
}

}
