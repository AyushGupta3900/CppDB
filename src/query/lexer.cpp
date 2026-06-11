#include "query/Lexer.hpp"

#include <array>
#include <cctype>
#include <utility>

namespace cppdb {

namespace {

constexpr std::array<const char*, 15> kKeywords = {
    "SELECT", "FROM", "WHERE", "INSERT", "INTO", "VALUES", "DELETE",
    "CREATE", "DROP",  "TABLE", "INT",   "TEXT",  "AND",    "UPDATE", "SET",
};

bool isKeyword(const std::string& upper) {
    for (const char* keyword : kKeywords) {
        if (upper == keyword) return true;
    }
    return false;
}

bool isIdentStart(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool isIdentChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

bool isDigit(char c) { return std::isdigit(static_cast<unsigned char>(c)); }

}  // namespace

Lexer::Lexer(std::string input) : input_(std::move(input)) {}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    std::size_t i = 0;
    const std::size_t n = input_.size();

    while (i < n) {
        const char c = input_[i];
        const std::size_t start = i;

        if (std::isspace(static_cast<unsigned char>(c))) {
            ++i;
            continue;
        }

        if (isIdentStart(c)) {
            while (i < n && isIdentChar(input_[i])) ++i;
            std::string word = input_.substr(start, i - start);
            std::string upper = word;
            for (char& ch : upper) ch = static_cast<char>(std::toupper(
                                       static_cast<unsigned char>(ch)));
            if (isKeyword(upper)) {
                tokens.push_back({TokenType::KEYWORD, std::move(upper), start});
            } else {
                tokens.push_back({TokenType::IDENTIFIER, std::move(word), start});
            }
            continue;
        }

        if (isDigit(c) || (c == '-' && i + 1 < n && isDigit(input_[i + 1]))) {
            ++i;  // first digit or the minus sign
            while (i < n && isDigit(input_[i])) ++i;
            tokens.push_back({TokenType::NUMBER, input_.substr(start, i - start), start});
            continue;
        }

        if (c == '\'' || c == '"') {
            const char quote = c;
            ++i;
            std::string content;
            bool terminated = false;
            while (i < n) {
                if (input_[i] == quote) {
                    if (i + 1 < n && input_[i + 1] == quote) {  // SQL escape: '' -> '
                        content += quote;
                        i += 2;
                        continue;
                    }
                    terminated = true;
                    ++i;  // closing quote
                    break;
                }
                content += input_[i++];
            }
            if (!terminated) {
                throw QueryError("unterminated string literal at position " +
                                 std::to_string(start));
            }
            tokens.push_back({TokenType::STRING, std::move(content), start});
            continue;
        }

        if (c == '=' ) {
            tokens.push_back({TokenType::OPERATOR, "=", start});
            ++i;
            continue;
        }
        if (c == '<' || c == '>') {
            std::string op(1, c);
            ++i;
            if (i < n && input_[i] == '=') {
                op += '=';
                ++i;
            }
            tokens.push_back({TokenType::OPERATOR, std::move(op), start});
            continue;
        }
        if (c == '!') {
            if (i + 1 < n && input_[i + 1] == '=') {
                tokens.push_back({TokenType::OPERATOR, "!=", start});
                i += 2;
                continue;
            }
            throw QueryError("unexpected '!' at position " + std::to_string(start));
        }

        if (c == '(' || c == ')' || c == ',' || c == '*') {
            tokens.push_back({TokenType::PUNCTUATION, std::string(1, c), start});
            ++i;
            continue;
        }

        throw QueryError(std::string("unexpected character '") + c +
                         "' at position " + std::to_string(start));
    }

    tokens.push_back({TokenType::END, "", n});
    return tokens;
}

}  // namespace cppdb
