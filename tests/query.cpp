#include "query/QueryExecutor.hpp"

#include <string>

#include "core/Database.hpp"
#include "query/Lexer.hpp"
#include "query/Parser.hpp"
#include "test_framework.hpp"

using cppdb::Database;
using cppdb::Lexer;
using cppdb::QueryError;
using cppdb::QueryExecutor;
using cppdb::Token;
using cppdb::TokenType;

namespace {

bool isError(const std::string& reply) { return reply.rfind("ERROR:", 0) == 0; }

}  // namespace

static void testLexer() {
    auto tokens = Lexer("SELECT * FROM users WHERE name = 'Al ice'").tokenize();
    CHECK_EQ(tokens.size(), 9u);  // 8 + END
    CHECK(tokens[0].type == TokenType::KEYWORD);
    CHECK_EQ(tokens[0].value, "SELECT");
    CHECK(tokens[1].type == TokenType::PUNCTUATION);
    CHECK(tokens[3].type == TokenType::IDENTIFIER);
    CHECK_EQ(tokens[3].value, "users");
    CHECK(tokens[6].type == TokenType::OPERATOR);
    CHECK(tokens[7].type == TokenType::STRING);
    CHECK_EQ(tokens[7].value, "Al ice");  // quotes stripped, space kept
    CHECK(tokens[8].type == TokenType::END);

    // Keywords are case-insensitive and normalized; identifiers keep case
    auto mixed = Lexer("select Id from Users").tokenize();
    CHECK_EQ(mixed[0].value, "SELECT");
    CHECK_EQ(mixed[1].value, "Id");
    CHECK_EQ(mixed[3].value, "Users");

    // Numbers, including negative; two-char operators
    auto nums = Lexer("WHERE age >= -42").tokenize();
    CHECK_EQ(nums[2].value, ">=");
    CHECK(nums[3].type == TokenType::NUMBER);
    CHECK_EQ(nums[3].value, "-42");

    CHECK_THROWS(Lexer("SELECT ; FROM t").tokenize(), QueryError);
    CHECK_THROWS(Lexer("SELECT 'unterminated").tokenize(), QueryError);
    CHECK_THROWS(Lexer("WHERE a ! b").tokenize(), QueryError);
}

static void testParserErrors() {
    auto parse = [](const std::string& q) {
        cppdb::Parser parser(Lexer(q).tokenize());
        return parser.parse();
    };
    CHECK_THROWS(parse(""), QueryError);
    CHECK_THROWS(parse("SELECT FROM users"), QueryError);
    CHECK_THROWS(parse("SELECT * users"), QueryError);
    CHECK_THROWS(parse("INSERT INTO users (id) VALUES (1, 2)"), QueryError);
    CHECK_THROWS(parse("CREATE TABLE t (id FLOAT)"), QueryError);
    CHECK_THROWS(parse("DELETE FROM"), QueryError);
    CHECK_THROWS(parse("SELECT * FROM users garbage"), QueryError);  // trailing input
    CHECK_THROWS(parse("UPDATE users"), QueryError);                  // unsupported
    CHECK_THROWS(parse("SELECT * FROM users WHERE id ="), QueryError);
}

static void testExecutorEndToEnd() {
    Database db;
    QueryExecutor exec(db);

    // CREATE
    CHECK_EQ(exec.execute("CREATE TABLE users (id INT, name TEXT, age INT)"), "OK");
    CHECK(isError(exec.execute("CREATE TABLE users (id INT)")));     // duplicate
    CHECK(isError(exec.execute("CREATE TABLE bad (name TEXT)")));    // no INT id
    CHECK(isError(exec.execute("CREATE TABLE worse (id INT, id INT)")));

    // INSERT
    CHECK_EQ(exec.execute("INSERT INTO users (id, name, age) VALUES (1, 'Alice', 30)"), "OK");
    CHECK_EQ(exec.execute("insert into users (id, name, age) values (2, \"Bob\", 25)"), "OK");
    CHECK_EQ(exec.execute("INSERT INTO users (id, name, age) VALUES (3, 'Carol', 35)"), "OK");
    CHECK(isError(exec.execute("INSERT INTO users (id, name, age) VALUES (1, 'Dup', 1)")));
    CHECK(isError(exec.execute("INSERT INTO users (id, name) VALUES (4, 'NoAge')")));  // incomplete
    CHECK(isError(exec.execute("INSERT INTO users (id, name, age) VALUES ('x', 'E', 1)")));  // type
    CHECK(isError(exec.execute("INSERT INTO nope (id) VALUES (1)")));

    // SELECT * — ordered by id, schema-ordered columns
    CHECK_EQ(exec.execute("SELECT * FROM users WHERE id = 1"),
             "id=1, name=Alice, age=30\n(1 row)");
    CHECK_EQ(exec.execute("SELECT * FROM users"),
             "id=1, name=Alice, age=30\nid=2, name=Bob, age=25\nid=3, name=Carol, age=35\n(3 rows)");
    CHECK_EQ(exec.execute("SELECT * FROM users WHERE id = 99"), "(0 rows)");

    // Projection
    CHECK_EQ(exec.execute("SELECT name FROM users WHERE id = 2"), "name=Bob\n(1 row)");
    CHECK_EQ(exec.execute("SELECT age, name FROM users WHERE id = 3"),
             "age=35, name=Carol\n(1 row)");
    CHECK(isError(exec.execute("SELECT email FROM users")));

    // WHERE on primary key ranges (B-tree fast path)
    CHECK_EQ(exec.execute("SELECT name FROM users WHERE id < 3"),
             "name=Alice\nname=Bob\n(2 rows)");
    CHECK_EQ(exec.execute("SELECT name FROM users WHERE id >= 2"),
             "name=Bob\nname=Carol\n(2 rows)");
    CHECK_EQ(exec.execute("SELECT name FROM users WHERE id != 2"),
             "name=Alice\nname=Carol\n(2 rows)");

    // WHERE on non-key columns: INT compares numerically, TEXT lexically
    CHECK_EQ(exec.execute("SELECT name FROM users WHERE age > 26"),
             "name=Alice\nname=Carol\n(2 rows)");
    CHECK_EQ(exec.execute("SELECT id FROM users WHERE name = 'Bob'"), "id=2\n(1 row)");
    CHECK(isError(exec.execute("SELECT * FROM users WHERE age = 'old'")));  // type mismatch
    CHECK(isError(exec.execute("SELECT * FROM users WHERE email = 'x'")));  // no column

    // DELETE
    CHECK_EQ(exec.execute("DELETE FROM users WHERE id = 2"), "OK (1 row deleted)");
    CHECK_EQ(exec.execute("DELETE FROM users WHERE id = 2"), "OK (0 rows deleted)");
    CHECK_EQ(exec.execute("SELECT * FROM users WHERE id = 2"), "(0 rows)");
    CHECK_EQ(exec.execute("DELETE FROM users WHERE age >= 30"), "OK (2 rows deleted)");
    CHECK_EQ(exec.execute("SELECT * FROM users"), "(0 rows)");

    // Reinsert after delete-all works (cache invalidation held up)
    CHECK_EQ(exec.execute("INSERT INTO users (id, name, age) VALUES (1, 'New', 20)"), "OK");
    CHECK_EQ(exec.execute("SELECT name FROM users WHERE id = 1"), "name=New\n(1 row)");
    CHECK_EQ(exec.execute("DELETE FROM users"), "OK (1 row deleted)");

    // DROP
    CHECK_EQ(exec.execute("DROP TABLE users"), "OK");
    CHECK(isError(exec.execute("DROP TABLE users")));
    CHECK(isError(exec.execute("SELECT * FROM users")));

    // Malformed queries come back as errors, never exceptions
    CHECK(isError(exec.execute("")));
    CHECK(isError(exec.execute("EXPLODE")));
    CHECK(isError(exec.execute("SELECT * FROM")));
}

int main() {
    testLexer();
    testParserErrors();
    testExecutorEndToEnd();
    return testfw::testSummary("query");
}
