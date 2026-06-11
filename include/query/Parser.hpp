#pragma once

#include <string>
#include <vector>

#include "query/Ast.hpp"
#include "query/Token.hpp"

namespace cppdb {

// Recursive-descent parser over the lexer's token stream. One public entry
// point per query; throws QueryError with the offending position on any
// grammar violation, including trailing garbage after a valid statement.
class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    Statement parse();

private:
    CreateTableStatement parseCreate();
    DropTableStatement parseDrop();
    InsertStatement parseInsert();
    SelectStatement parseSelect();
    UpdateStatement parseUpdate();
    DeleteStatement parseDelete();

    WhereList parseOptionalWhere();  // WHERE cond (AND cond)* | nothing
    std::vector<std::string> parseIdentifierList();
    std::string parseLiteral();

    const Token& peek() const;
    const Token& advance();
    bool peekIsKeyword(const std::string& keyword) const;
    const Token& expect(TokenType type, const std::string& what);
    void expectKeyword(const std::string& keyword);
    void expectPunctuation(char c);
    void expectEnd();
    [[noreturn]] void fail(const std::string& message) const;

    std::vector<Token> tokens_;
    std::size_t pos_ = 0;
};

}  // namespace cppdb
