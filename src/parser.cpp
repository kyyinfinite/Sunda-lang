#include "parser.h"

#include <utility>

namespace sunda {

namespace {

std::string unescape(std::string_view lexeme) {
    std::string out;
    out.reserve(lexeme.size());
    for (size_t index = 1; index + 1 < lexeme.size(); ++index) {
        char c = lexeme[index];
        if (c == '\\' && index + 2 < lexeme.size()) {
            ++index;
            switch (lexeme[index]) {
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case '0': out += '\0'; break;
                case '\\': out += '\\'; break;
                case '"': out += '"'; break;
                default:
                    out += '\\';
                    out += lexeme[index];
                    break;
            }
        } else {
            out += c;
        }
    }
    return out;
}

TokenType compoundBase(TokenType type) {
    switch (type) {
        case TokenType::PlusEqual: return TokenType::Plus;
        case TokenType::MinusEqual: return TokenType::Minus;
        case TokenType::StarEqual: return TokenType::Star;
        default: return TokenType::Slash;
    }
}

}

Parser::Parser(std::vector<Token> tokenList) : tokens(std::move(tokenList)) {}

const Token& Parser::advance() {
    const Token& token = tokens[position];
    if (token.type != TokenType::Eof) ++position;
    return token;
}

bool Parser::match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

const Token& Parser::consume(TokenType type, const char* message) {
    if (!check(type)) fail(peek(), message);
    return advance();
}

void Parser::skipNewlines() {
    while (check(TokenType::Newline)) advance();
}

void Parser::fail(const Token& token, std::string message) const {
    throw SyntaxError{token.line, token.column, std::move(message)};
}

Program Parser::parse() {
    Program program;
    while (!check(TokenType::Eof)) {
        if (match(TokenType::Newline)) continue;
        program.push_back(statement());
    }
    return program;
}

StmtPtr Parser::statement() {
    switch (peek().type) {
        case TokenType::Pagawean: return functionStatement();
        case TokenType::Kelas: return classStatement();
        case TokenType::Impor: return importStatement();
        case TokenType::Cobaan: return tryStatement();
        case TokenType::Gagal: return throwStatement();
        case TokenType::Balikeun: return returnStatement();
        case TokenType::LeftBrace: return blockStatement();
        case TokenType::Lamun: return ifStatement();
        case TokenType::Salila: return whileStatement();
        case TokenType::Pikeun: return forStatement();
        case TokenType::Eureun:
        case TokenType::Teruskeun: return jumpStatement();
        default: return expressionStatement();
    }
}

void Parser::endStatement() {
    if (match(TokenType::Newline)) return;
    if (!check(TokenType::Eof) && !check(TokenType::RightBrace)) fail(peek(), "kurang baris anyar");
}

StmtPtr Parser::blockStatement() {
    const Token open = peek();
    consume(TokenType::LeftBrace, "kurang {");
    std::vector<StmtPtr> body;
    skipNewlines();
    while (!check(TokenType::RightBrace) && !check(TokenType::Eof)) {
        body.push_back(statement());
        skipNewlines();
    }
    if (check(TokenType::Eof)) fail(open, "kurang }");
    advance();
    return std::make_unique<BlockStmt>(std::move(body), open.line);
}

void Parser::parseParams(FunctionStmt& fn) {
    consume(TokenType::LeftParen, "kurang (");
    skipNewlines();
    if (match(TokenType::RightParen)) return;
    bool seenDefault = false;
    for (;;) {
        skipNewlines();
        if (match(TokenType::Ellipsis)) {
            if (fn.restIndex >= 0) fail(peek(), "hiji wungkul parameter sésa");
            const Token& name = consume(TokenType::Identifier, "kurang ngaran parameter sésa");
            fn.restIndex = static_cast<int>(fn.params.size());
            fn.restName = std::string(name.lexeme);
            skipNewlines();
            consume(TokenType::RightParen, "kurang )");
            return;
        }
        const Token& name = consume(TokenType::Identifier, "kurang ngaran parameter");
        Param param;
        param.name = std::string(name.lexeme);
        param.line = name.line;
        skipNewlines();
        if (match(TokenType::Equal)) {
            skipNewlines();
            param.defaultValue = expression();
            seenDefault = true;
        } else if (seenDefault) {
            fail(name, "parameter ieu kudu boga nilai bawaan");
        }
        for (const Param& existing : fn.params) {
            if (existing.name == param.name) fail(name, "ngaran parameter dobel");
        }
        fn.params.push_back(std::move(param));
        skipNewlines();
        if (!match(TokenType::Comma)) break;
    }
    skipNewlines();
    consume(TokenType::RightParen, "kurang )");
}

StmtPtr Parser::functionStatement() {
    const Token keyword = advance();
    const Token& name = consume(TokenType::Identifier, "kurang ngaran pagawean");
    auto fn = std::make_unique<FunctionStmt>(std::string(name.lexeme), keyword.line);
    parseParams(*fn);
    fn->body = blockStatement();
    return fn;
}

StmtPtr Parser::classStatement() {
    const Token keyword = advance();
    const Token& name = consume(TokenType::Identifier, "kurang ngaran kelas");
    auto klass = std::make_unique<ClassStmt>(std::string(name.lexeme), keyword.line);
    if (match(TokenType::Turunan)) {
        const Token& parent = consume(TokenType::Identifier, "kurang ngaran kelas inditna");
        klass->hasSuperclass = true;
        klass->superclass = std::string(parent.lexeme);
    }
    consume(TokenType::LeftBrace, "kurang {");
    skipNewlines();
    while (!check(TokenType::RightBrace) && !check(TokenType::Eof)) {
        if (match(TokenType::Newline)) continue;
        if (!check(TokenType::Pagawean)) fail(peek(), "dina kelas ngan sah ngawengku pagawean");
        const Token fnKeyword = advance();
        if (!check(TokenType::Identifier) && !check(TokenType::Awal)) {
            fail(peek(), "kurang ngaran metoda");
        }
        const Token& methodName = advance();
        auto method = std::make_unique<FunctionStmt>(std::string(methodName.lexeme), fnKeyword.line);
        parseParams(*method);
        method->body = blockStatement();
        klass->methods.push_back(std::move(method));
        skipNewlines();
    }
    if (check(TokenType::Eof)) fail(keyword, "kurang }");
    advance();
    return klass;
}

StmtPtr Parser::importStatement() {
    const Token keyword = advance();
    const Token& name = consume(TokenType::Identifier, "kurang ngaran modul");
    endStatement();
    return std::make_unique<ImportStmt>(std::string(name.lexeme), keyword.line);
}

StmtPtr Parser::tryStatement() {
    const Token keyword = advance();
    auto node = std::make_unique<TryStmt>(keyword.line);
    node->body = blockStatement();
    if (!check(TokenType::Gagal)) fail(peek(), "cobaan kudu dituturkeun ku gagal");
    advance();
    if (match(TokenType::LeftParen)) {
        const Token& name = consume(TokenType::Identifier, "kurang ngaran variabel galat");
        node->hasCatchVar = true;
        node->catchVar = std::string(name.lexeme);
        consume(TokenType::RightParen, "kurang )");
    }
    node->handler = blockStatement();
    return node;
}

StmtPtr Parser::throwStatement() {
    const Token keyword = advance();
    ExprPtr value = expression();
    endStatement();
    return std::make_unique<ThrowStmt>(std::move(value), keyword.line);
}

StmtPtr Parser::returnStatement() {
    const Token keyword = advance();
    auto node = std::make_unique<ReturnStmt>(keyword.line);
    if (!check(TokenType::Newline) && !check(TokenType::RightBrace) && !check(TokenType::Eof)) {
        node->value = expression();
    }
    endStatement();
    return node;
}

StmtPtr Parser::expressionStatement() {
    int line = peek().line;
    ExprPtr expr = expression();
    endStatement();
    return std::make_unique<ExpressionStmt>(std::move(expr), line);
}

StmtPtr Parser::ifStatement() {
    const Token keyword = advance();
    ExprPtr condition = expression();
    StmtPtr thenBranch = blockStatement();
    StmtPtr elseBranch;
    if (check(TokenType::Newline) && tokens[position + 1].type == TokenType::Lamunteu) advance();
    if (match(TokenType::Lamunteu)) {
        elseBranch = check(TokenType::Lamun) ? ifStatement() : blockStatement();
    }
    return std::make_unique<IfStmt>(std::move(condition), std::move(thenBranch), std::move(elseBranch), keyword.line);
}

StmtPtr Parser::whileStatement() {
    const Token keyword = advance();
    ExprPtr condition = expression();
    StmtPtr body = blockStatement();
    return std::make_unique<WhileStmt>(std::move(condition), std::move(body), keyword.line);
}

StmtPtr Parser::forStatement() {
    const Token keyword = advance();
    const Token name = consume(TokenType::Identifier, "kurang ngaran variabel");
    consume(TokenType::Dina, "kurang dina");
    ExprPtr iterable = expression();
    StmtPtr body = blockStatement();
    return std::make_unique<ForStmt>(
        std::string(name.lexeme), std::move(iterable), std::move(body), keyword.line);
}

StmtPtr Parser::jumpStatement() {
    const Token keyword = advance();
    endStatement();
    if (keyword.type == TokenType::Eureun) return std::make_unique<BreakStmt>(keyword.line);
    return std::make_unique<ContinueStmt>(keyword.line);
}

std::vector<ExprPtr> Parser::argumentList() {
    std::vector<ExprPtr> arguments;
    skipNewlines();
    if (!check(TokenType::RightParen)) {
        do {
            skipNewlines();
            arguments.push_back(expression());
            skipNewlines();
        } while (match(TokenType::Comma));
    }
    consume(TokenType::RightParen, "kurang )");
    return arguments;
}

ExprPtr Parser::expression() { return parsePrecedence(PrecAssignment); }

int Parser::infixPrecedence(TokenType type) {
    switch (type) {
        case TokenType::Equal:
        case TokenType::PlusEqual:
        case TokenType::MinusEqual:
        case TokenType::StarEqual:
        case TokenType::SlashEqual:
            return PrecAssignment;
        case TokenType::Atawa:
            return PrecOr;
        case TokenType::Jeung:
            return PrecAnd;
        case TokenType::EqualEqual:
        case TokenType::BangEqual:
            return PrecEquality;
        case TokenType::Less:
        case TokenType::LessEqual:
        case TokenType::Greater:
        case TokenType::GreaterEqual:
            return PrecComparison;
        case TokenType::Plus:
        case TokenType::Minus:
            return PrecTerm;
        case TokenType::Star:
        case TokenType::Slash:
        case TokenType::Percent:
            return PrecFactor;
        case TokenType::StarStar:
            return PrecPower;
        case TokenType::LeftParen:
        case TokenType::LeftBracket:
        case TokenType::Dot:
            return PrecCall;
        default:
            return PrecNone;
    }
}

ExprPtr Parser::parsePrecedence(int minimum) {
    ExprPtr left = prefix();
    while (minimum <= infixPrecedence(peek().type)) {
        const Token op = advance();
        left = infix(std::move(left), op);
    }
    return left;
}

ExprPtr Parser::prefix() {
    const Token token = advance();
    switch (token.type) {
        case TokenType::Number:
            return std::make_unique<NumberExpr>(token.number, token.line, token.column);
        case TokenType::String:
            return std::make_unique<StringExpr>(unescape(token.lexeme), token.line, token.column);
        case TokenType::Bener:
        case TokenType::Salah:
        case TokenType::Kosong:
            return std::make_unique<LiteralExpr>(token.type, token.line, token.column);
        case TokenType::Identifier:
            return std::make_unique<VariableExpr>(std::string(token.lexeme), token.line, token.column);
        case TokenType::Ieu:
            return std::make_unique<VariableExpr>(std::string(token.lexeme), token.line, token.column);
        case TokenType::LeftParen:
            return parenExpr(token);
        case TokenType::LeftBracket:
            return listExpr(token);
        case TokenType::LeftBrace:
            return braceExpr(token);
        case TokenType::Minus:
            return std::make_unique<UnaryExpr>(
                TokenType::Minus, parsePrecedence(PrecUnary), token.line, token.column);
        case TokenType::Lain:
            return std::make_unique<UnaryExpr>(
                TokenType::Lain, parsePrecedence(PrecNot), token.line, token.column);
        case TokenType::Induk: {
            consume(TokenType::Dot, "kurang . sanggeus induk");
            if (!check(TokenType::Identifier) && !check(TokenType::Awal)) {
                fail(peek(), "kurang ngaran metoda induk");
            }
            const Token& method = advance();
            return std::make_unique<SuperExpr>(std::string(method.lexeme), token.line, token.column);
        }
        default:
            fail(token, "ekspresi teu kapanggih");
    }
}

ExprPtr Parser::parenExpr(const Token& open) {
    skipNewlines();
    if (match(TokenType::RightParen)) {
        return std::make_unique<TupleExpr>(std::vector<ExprPtr>{}, open.line, open.column);
    }
    ExprPtr first = expression();
    skipNewlines();
    if (check(TokenType::Comma)) {
        std::vector<ExprPtr> elements;
        elements.push_back(std::move(first));
        while (match(TokenType::Comma)) {
            skipNewlines();
            if (check(TokenType::RightParen)) break;
            elements.push_back(expression());
            skipNewlines();
        }
        consume(TokenType::RightParen, "kurang )");
        return std::make_unique<TupleExpr>(std::move(elements), open.line, open.column);
    }
    consume(TokenType::RightParen, "kurang )");
    return first;
}

ExprPtr Parser::listExpr(const Token& open) {
    std::vector<ExprPtr> elements;
    skipNewlines();
    if (!check(TokenType::RightBracket)) {
        do {
            skipNewlines();
            if (check(TokenType::RightBracket)) break;
            elements.push_back(expression());
            skipNewlines();
        } while (match(TokenType::Comma));
    }
    consume(TokenType::RightBracket, "kurang ]");
    return std::make_unique<ListExpr>(std::move(elements), open.line, open.column);
}

ExprPtr Parser::braceExpr(const Token& open) {
    skipNewlines();
    if (match(TokenType::RightBrace)) {
        return std::make_unique<MapExpr>(open.line, open.column);
    }
    ExprPtr first = expression();
    skipNewlines();
    if (match(TokenType::Colon)) {
        skipNewlines();
        ExprPtr firstValue = expression();
        auto map = std::make_unique<MapExpr>(open.line, open.column);
        map->entries.push_back({std::move(first), std::move(firstValue)});
        skipNewlines();
        while (match(TokenType::Comma)) {
            skipNewlines();
            if (check(TokenType::RightBrace)) break;
            ExprPtr key = expression();
            skipNewlines();
            consume(TokenType::Colon, "kurang :");
            skipNewlines();
            ExprPtr value = expression();
            map->entries.push_back({std::move(key), std::move(value)});
            skipNewlines();
        }
        consume(TokenType::RightBrace, "kurang }");
        return map;
    }
    auto set = std::make_unique<SetExpr>(open.line, open.column);
    set->elements.push_back(std::move(first));
    while (match(TokenType::Comma)) {
        skipNewlines();
        if (check(TokenType::RightBrace)) break;
        set->elements.push_back(expression());
        skipNewlines();
    }
    consume(TokenType::RightBrace, "kurang }");
    return set;
}

ExprPtr Parser::indexOrSlice(ExprPtr object, const Token& open) {
    skipNewlines();
    if (match(TokenType::Colon)) {
        auto node = std::make_unique<SliceExpr>(std::move(object), open.line, open.column);
        if (!check(TokenType::RightBracket)) {
            skipNewlines();
            node->end = expression();
            node->hasEnd = true;
        }
        skipNewlines();
        consume(TokenType::RightBracket, "kurang ]");
        return node;
    }
    ExprPtr first = expression();
    skipNewlines();
    if (match(TokenType::Colon)) {
        auto node = std::make_unique<SliceExpr>(std::move(object), open.line, open.column);
        node->start = std::move(first);
        node->hasStart = true;
        if (!check(TokenType::RightBracket)) {
            skipNewlines();
            node->end = expression();
            node->hasEnd = true;
        }
        skipNewlines();
        consume(TokenType::RightBracket, "kurang ]");
        return node;
    }
    consume(TokenType::RightBracket, "kurang ]");
    return std::make_unique<IndexExpr>(std::move(object), std::move(first), open.line, open.column);
}

ExprPtr Parser::infix(ExprPtr left, const Token& op) {
    switch (op.type) {
        case TokenType::LeftParen:
            return finishCall(std::move(left), op);
        case TokenType::LeftBracket:
            return indexOrSlice(std::move(left), op);
        case TokenType::Dot: {
            const Token& name = check(TokenType::Awal) ? peek() : peek();
            if (name.type != TokenType::Identifier && name.type != TokenType::Awal) {
                fail(name, "kurang ngaran sipat");
            }
            advance();
            return std::make_unique<PropertyExpr>(
                std::move(left), std::string(name.lexeme), op.line, op.column);
        }
        case TokenType::Equal:
        case TokenType::PlusEqual:
        case TokenType::MinusEqual:
        case TokenType::StarEqual:
        case TokenType::SlashEqual:
            return assignment(std::move(left), op);
        case TokenType::StarStar: {
            ExprPtr right = parsePrecedence(PrecPower);
            return std::make_unique<BinaryExpr>(op.type, std::move(left), std::move(right), op.line, op.column);
        }
        case TokenType::Jeung:
        case TokenType::Atawa: {
            ExprPtr right = parsePrecedence(infixPrecedence(op.type) + 1);
            return std::make_unique<LogicalExpr>(op.type, std::move(left), std::move(right), op.line, op.column);
        }
        default: {
            ExprPtr right = parsePrecedence(infixPrecedence(op.type) + 1);
            return std::make_unique<BinaryExpr>(op.type, std::move(left), std::move(right), op.line, op.column);
        }
    }
}

ExprPtr Parser::assignment(ExprPtr left, const Token& op) {
    if (left->kind != ExprKind::Variable && left->kind != ExprKind::Property &&
        left->kind != ExprKind::Index) {
        fail(op, "target tugas teu sah");
    }
    ExprPtr value = parsePrecedence(PrecAssignment);
    auto node = std::make_unique<AssignExpr>(std::move(left), std::move(value), op.line, op.column);
    if (op.type != TokenType::Equal) node->targetIsCompound = true;
    node->compoundOp = compoundBase(op.type);
    return node;
}

ExprPtr Parser::finishCall(ExprPtr callee, const Token& open) {
    std::vector<ExprPtr> arguments = argumentList();
    return std::make_unique<CallExpr>(std::move(callee), std::move(arguments), open.line, open.column);
}

}
