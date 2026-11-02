#include "formatter.h"

#include <cctype>

namespace sunda {

namespace {

bool isBinaryOp(TokenType type) {
    switch (type) {
        case TokenType::Plus:
        case TokenType::Minus:
        case TokenType::Star:
        case TokenType::Slash:
        case TokenType::Percent:
        case TokenType::StarStar:
        case TokenType::EqualEqual:
        case TokenType::BangEqual:
        case TokenType::Less:
        case TokenType::LessEqual:
        case TokenType::Greater:
        case TokenType::GreaterEqual:
        case TokenType::Jeung:
        case TokenType::Atawa:
            return true;
        default:
            return false;
    }
}

bool isAssignOp(TokenType type) {
    switch (type) {
        case TokenType::Equal:
        case TokenType::PlusEqual:
        case TokenType::MinusEqual:
        case TokenType::StarEqual:
        case TokenType::SlashEqual:
            return true;
        default:
            return false;
    }
}

bool isDeclarationKeyword(TokenType type) {
    switch (type) {
        case TokenType::Lamun:
        case TokenType::Lamunteu:
        case TokenType::Salila:
        case TokenType::Pikeun:
        case TokenType::Pagawean:
        case TokenType::Kelas:
        case TokenType::Balikeun:
        case TokenType::Impor:
        case TokenType::Cobaan:
        case TokenType::Gagal:
        case TokenType::Lain:
        case TokenType::Turunan:
        case TokenType::Dina:
        case TokenType::Ieu:
        case TokenType::Induk:
            return true;
        default:
            return false;
    }
}

bool spaceNeededBefore(TokenType previous, TokenType current, bool previousIsKeyword) {
    switch (previous) {
        case TokenType::LeftParen:
        case TokenType::LeftBracket:
        case TokenType::Comma:
        case TokenType::Colon:
        case TokenType::Ellipsis:
        case TokenType::Dot:
            return false;
        default:
            break;
    }
    switch (current) {
        case TokenType::RightParen:
        case TokenType::RightBracket:
        case TokenType::LeftBracket:
        case TokenType::Comma:
        case TokenType::Colon:
        case TokenType::Dot:
            return false;
        case TokenType::LeftParen:
            return previousIsKeyword;
        case TokenType::Ellipsis:
            return previous == TokenType::LeftParen;
        default:
            return true;
    }
}

std::string commentText(std::string_view lexeme) {
    std::string text(lexeme);
    size_t end = text.find_last_not_of(" \t\r");
    if (end != std::string::npos) text.erase(end + 1);
    else text = "#";
    return text;
}

}

std::string formatSource(std::string_view source) {
    Lexer lexer(source);
    std::vector<FormatToken> tokens = lexer.tokenizeForFormat();

    std::string out;
    int indent = 0;
    bool lineOpen = false;
    bool previousWasNewline = true;
    TokenType previousType = TokenType::Eof;
    bool previousKeyword = false;

    auto closeLine = [&]() {
        if (lineOpen) {
            while (!out.empty() && out.back() == ' ') out.pop_back();
            out += '\n';
            lineOpen = false;
        }
    };

    size_t index = 0;
    while (index < tokens.size()) {
        FormatToken token = tokens[index];
        if (token.comment) {
            if (!lineOpen) {
                if (!previousWasNewline) out += '\n';
                out.append(static_cast<size_t>(indent) * 4, ' ');
                lineOpen = true;
            } else {
                out += ' ';
            }
            out += commentText(token.lexeme);
            closeLine();
            previousWasNewline = false;
            lineOpen = false;
            ++index;
            continue;
        }
        if (token.type == TokenType::Newline) {
            closeLine();
            lineOpen = false;
            previousWasNewline = true;
            previousType = TokenType::Newline;
            previousKeyword = false;
            ++index;
            while (index < tokens.size() && tokens[index].type == TokenType::Newline &&
                   !tokens[index].comment) {
                ++index;
            }
            continue;
        }

        if (token.type == TokenType::RightBrace) {
            bool followedByElse = false;
            size_t next = index + 1;
            while (next < tokens.size() && tokens[next].type == TokenType::Newline) ++next;
            if (next < tokens.size() && !tokens[next].comment) {
                TokenType nextType = tokens[next].type;
                if (nextType == TokenType::Lamunteu) {
                    followedByElse = true;
                } else if (nextType == TokenType::Gagal && next + 1 < tokens.size()) {
                    TokenType afterGagal = tokens[next + 1].type;
                    if (afterGagal == TokenType::LeftParen) {
                        followedByElse = true;
                    } else if (afterGagal == TokenType::Identifier && next + 2 < tokens.size() &&
                               tokens[next + 2].type == TokenType::LeftBrace) {
                        followedByElse = true;
                    }
                }
            }
            indent = indent > 0 ? indent - 1 : 0;
            if (lineOpen) {
                while (!out.empty() && out.back() == ' ') out.pop_back();
                out += ' ';
            } else {
                out.append(static_cast<size_t>(indent) * 4, ' ');
            }
            out += '}';
            lineOpen = true;
            if (!followedByElse) {
                closeLine();
                previousWasNewline = true;
            }
            previousType = TokenType::RightBrace;
            previousKeyword = false;
            ++index;
            continue;
        }

        if (!lineOpen) {
            out.append(static_cast<size_t>(indent) * 4, ' ');
            lineOpen = true;
        } else if (!out.empty() && out.back() != ' ' &&
                   spaceNeededBefore(previousType, token.type, previousKeyword)) {
            out += ' ';
        }

        out += std::string(token.lexeme);

        switch (token.type) {
            case TokenType::LeftBrace:
                ++indent;
                break;
            case TokenType::Comma:
            case TokenType::Colon:
            case TokenType::Dina:
            case TokenType::Turunan:
            case TokenType::Impor:
            case TokenType::Ellipsis:
            case TokenType::Lain:
                out += ' ';
                break;
            default:
                if (isBinaryOp(token.type) || isAssignOp(token.type) ||
                    isDeclarationKeyword(token.type)) {
                    out += ' ';
                }
                break;
        }
        previousType = token.type;
        previousKeyword = isDeclarationKeyword(token.type);
        previousWasNewline = false;
        ++index;
    }
    closeLine();
    return out;
}

}
