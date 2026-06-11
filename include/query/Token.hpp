#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>

namespace cppdb {

// Thrown for any malformed query — lexing, parsing, or execution-time
// validation (unknown table/column, type mismatch). The executor catches it
// and turns it into an "ERROR: ..." reply, so a bad query can never take the
// server down.
class QueryError : public std::runtime_error {
public:
    explicit QueryError(const std::string& message) : std::runtime_error(message) {}
};

enum class TokenType {
    KEYWORD,      // SELECT, FROM, CREATE, INT, ... (value stored uppercase)
    IDENTIFIER,   // table / column names (case preserved)
    NUMBER,       // integer literal, optional leading '-'
    STRING,       // quoted literal, quotes stripped
    OPERATOR,     // = != < <= > >=
    PUNCTUATION,  // ( ) , *
    END,          // end of input sentinel
};

struct Token {
    TokenType type;
    std::string value;
    std::size_t position;  // byte offset in the query, for error messages
};

}  // namespace cppdb
