#pragma once

#include <string>
#include <string_view>

namespace sunda {

enum class TokenType {
    LeftParen,
    RightParen,
    LeftBrace,
    RightBrace,
    LeftBracket,
    RightBracket,
    Comma,
    Dot,
    Colon,
    Ellipsis,
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    StarStar,
    Equal,
    EqualEqual,
    BangEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    PlusEqual,
    MinusEqual,
    StarEqual,
    SlashEqual,
    Identifier,
    Number,
    String,
    Lamun,
    Lamunteu,
    Salila,
    Pikeun,
    Dina,
    Pagawean,
    Balikeun,
    Eureun,
    Teruskeun,
    Kelas,
    Turunan,
    Ieu,
    Induk,
    Awal,
    Bener,
    Salah,
    Kosong,
    Jeung,
    Atawa,
    Lain,
    Impor,
    Cobaan,
    Gagal,
    Newline,
    Eof,
};

struct Token {
    TokenType type;
    std::string_view lexeme;
    int line;
    int column;
    double number;
};

struct SyntaxError {
    int line;
    int column;
    std::string message;
};

inline std::string_view sourceLine(std::string_view source, int line) {
    size_t begin = 0;
    for (int current = 1; current < line; ++current) {
        size_t newline = source.find('\n', begin);
        if (newline == std::string_view::npos) return {};
        begin = newline + 1;
    }
    size_t end = source.find('\n', begin);
    if (end == std::string_view::npos) end = source.size();
    std::string_view text = source.substr(begin, end - begin);
    if (!text.empty() && text.back() == '\r') text.remove_suffix(1);
    return text;
}

inline std::string formatError(std::string_view source, int line, int column, std::string_view message) {
    std::string out = "Aya nu salah di baris " + std::to_string(line) + ":\n    ";
    out += sourceLine(source, line);
    out += "\n    ";
    out.append(column > 1 ? static_cast<size_t>(column - 1) : 0, ' ');
    out += "^ ";
    out += message;
    out += "\n";
    return out;
}

}
