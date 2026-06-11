#pragma once

#include <string>
#include <vector>

#include "query/Token.hpp"

namespace cppdb {

// Hand-written tokenizer for the SQL subset. Keywords are recognized
// case-insensitively and normalized to uppercase; identifiers keep their
// spelling. Strings accept single or double quotes (no escape sequences).
// Throws QueryError on unexpected characters or unterminated strings.
class Lexer {
public:
    explicit Lexer(std::string input);

    // Always ends with an END token.
    std::vector<Token> tokenize();

private:
    std::string input_;
};

}  // namespace cppdb
