#include "query/Parser.hpp"

#include <utility>

namespace cppdb {

namespace {

CompareOp compareOpFromToken(const Token& token) {
    if (token.value == "=") return CompareOp::EQ;
    if (token.value == "!=") return CompareOp::NE;
    if (token.value == "<") return CompareOp::LT;
    if (token.value == "<=") return CompareOp::LE;
    if (token.value == ">") return CompareOp::GT;
    if (token.value == ">=") return CompareOp::GE;
    throw QueryError("unknown operator '" + token.value + "' at position " +
                     std::to_string(token.position));
}

}  // namespace

std::string toString(CompareOp op) {
    switch (op) {
        case CompareOp::EQ: return "=";
        case CompareOp::NE: return "!=";
        case CompareOp::LT: return "<";
        case CompareOp::LE: return "<=";
        case CompareOp::GT: return ">";
        case CompareOp::GE: return ">=";
    }
    return "?";
}

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

Statement Parser::parse() {
    const Token& first = peek();
    if (first.type != TokenType::KEYWORD) {
        fail("expected a statement (SELECT, INSERT, DELETE, CREATE, DROP)");
    }
    if (first.value == "CREATE") return parseCreate();
    if (first.value == "DROP") return parseDrop();
    if (first.value == "INSERT") return parseInsert();
    if (first.value == "SELECT") return parseSelect();
    if (first.value == "UPDATE") return parseUpdate();
    if (first.value == "DELETE") return parseDelete();
    fail("unsupported statement '" + first.value + "'");
}

// CREATE TABLE name (col TYPE, col TYPE, ...)
CreateTableStatement Parser::parseCreate() {
    expectKeyword("CREATE");
    expectKeyword("TABLE");
    CreateTableStatement stmt;
    stmt.table = expect(TokenType::IDENTIFIER, "table name").value;
    expectPunctuation('(');
    while (true) {
        ColumnDef def;
        def.name = expect(TokenType::IDENTIFIER, "column name").value;
        const Token& typeToken = expect(TokenType::KEYWORD, "column type");
        const auto type = dataTypeFromString(typeToken.value);
        if (!type) {
            fail("unknown column type '" + typeToken.value + "'");
        }
        def.type = *type;
        stmt.columns.push_back(std::move(def));
        if (peek().type == TokenType::PUNCTUATION && peek().value == ",") {
            advance();
            continue;
        }
        break;
    }
    expectPunctuation(')');
    expectEnd();
    return stmt;
}

// DROP TABLE name
DropTableStatement Parser::parseDrop() {
    expectKeyword("DROP");
    expectKeyword("TABLE");
    DropTableStatement stmt;
    stmt.table = expect(TokenType::IDENTIFIER, "table name").value;
    expectEnd();
    return stmt;
}

// INSERT INTO name (col, ...) VALUES (literal, ...)
InsertStatement Parser::parseInsert() {
    expectKeyword("INSERT");
    expectKeyword("INTO");
    InsertStatement stmt;
    stmt.table = expect(TokenType::IDENTIFIER, "table name").value;
    expectPunctuation('(');
    stmt.columns = parseIdentifierList();
    expectPunctuation(')');
    expectKeyword("VALUES");
    expectPunctuation('(');
    while (true) {
        stmt.values.push_back(parseLiteral());
        if (peek().type == TokenType::PUNCTUATION && peek().value == ",") {
            advance();
            continue;
        }
        break;
    }
    expectPunctuation(')');
    expectEnd();
    if (stmt.columns.size() != stmt.values.size()) {
        throw QueryError("INSERT has " + std::to_string(stmt.columns.size()) +
                         " columns but " + std::to_string(stmt.values.size()) +
                         " values");
    }
    return stmt;
}

// SELECT * | col, ... FROM name [WHERE col op literal]
SelectStatement Parser::parseSelect() {
    expectKeyword("SELECT");
    SelectStatement stmt;
    if (peek().type == TokenType::PUNCTUATION && peek().value == "*") {
        advance();  // empty column list means every column
    } else {
        stmt.columns = parseIdentifierList();
    }
    expectKeyword("FROM");
    stmt.table = expect(TokenType::IDENTIFIER, "table name").value;
    stmt.where = parseOptionalWhere();
    expectEnd();
    return stmt;
}

// UPDATE name SET col = literal, ... [WHERE col op literal AND ...]
UpdateStatement Parser::parseUpdate() {
    expectKeyword("UPDATE");
    UpdateStatement stmt;
    stmt.table = expect(TokenType::IDENTIFIER, "table name").value;
    expectKeyword("SET");
    while (true) {
        std::string column = expect(TokenType::IDENTIFIER, "column name").value;
        const Token& eq = expect(TokenType::OPERATOR, "'='");
        if (eq.value != "=") fail("expected '=' in SET assignment");
        stmt.assignments.emplace_back(std::move(column), parseLiteral());
        if (peek().type == TokenType::PUNCTUATION && peek().value == ",") {
            advance();
            continue;
        }
        break;
    }
    stmt.where = parseOptionalWhere();
    expectEnd();
    return stmt;
}

// DELETE FROM name [WHERE col op literal AND ...]
DeleteStatement Parser::parseDelete() {
    expectKeyword("DELETE");
    expectKeyword("FROM");
    DeleteStatement stmt;
    stmt.table = expect(TokenType::IDENTIFIER, "table name").value;
    stmt.where = parseOptionalWhere();
    expectEnd();
    return stmt;
}

WhereList Parser::parseOptionalWhere() {
    WhereList conditions;
    if (!peekIsKeyword("WHERE")) return conditions;
    advance();
    while (true) {
        WhereClause where;
        where.column = expect(TokenType::IDENTIFIER, "column name").value;
        where.op =
            compareOpFromToken(expect(TokenType::OPERATOR, "comparison operator"));
        where.value = parseLiteral();
        conditions.push_back(std::move(where));
        if (!peekIsKeyword("AND")) break;
        advance();
    }
    return conditions;
}

std::string Parser::parseLiteral() {
    const Token& token = peek();
    if (token.type != TokenType::NUMBER && token.type != TokenType::STRING) {
        fail("expected a literal value");
    }
    return advance().value;
}

std::vector<std::string> Parser::parseIdentifierList() {
    std::vector<std::string> names;
    names.push_back(expect(TokenType::IDENTIFIER, "column name").value);
    while (peek().type == TokenType::PUNCTUATION && peek().value == ",") {
        advance();
        names.push_back(expect(TokenType::IDENTIFIER, "column name").value);
    }
    return names;
}

const Token& Parser::peek() const { return tokens_[pos_]; }

const Token& Parser::advance() {
    const Token& token = tokens_[pos_];
    if (token.type != TokenType::END) ++pos_;  // never run past the sentinel
    return token;
}

bool Parser::peekIsKeyword(const std::string& keyword) const {
    return peek().type == TokenType::KEYWORD && peek().value == keyword;
}

const Token& Parser::expect(TokenType type, const std::string& what) {
    if (peek().type != type) fail("expected " + what);
    return advance();
}

void Parser::expectKeyword(const std::string& keyword) {
    if (!peekIsKeyword(keyword)) fail("expected " + keyword);
    advance();
}

void Parser::expectPunctuation(char c) {
    if (peek().type != TokenType::PUNCTUATION || peek().value != std::string(1, c)) {
        fail(std::string("expected '") + c + "'");
    }
    advance();
}

void Parser::expectEnd() {
    if (peek().type != TokenType::END) {
        fail("unexpected trailing input '" + peek().value + "'");
    }
}

void Parser::fail(const std::string& message) const {
    throw QueryError(message + " at position " + std::to_string(peek().position) +
                     (peek().type == TokenType::END ? " (end of query)"
                                                    : ", got '" + peek().value + "'"));
}

}  // namespace cppdb
