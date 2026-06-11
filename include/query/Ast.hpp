#pragma once

#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "core/Schema.hpp"

namespace cppdb {

enum class CompareOp { EQ, NE, LT, LE, GT, GE };

std::string toString(CompareOp op);

// `column <op> literal`. The literal is kept as the raw string; the executor
// type-checks it against the column when the query runs.
struct WhereClause {
    std::string column;
    CompareOp op;
    std::string value;
};

// A WHERE is a conjunction: every clause must hold (AND). Empty = no filter.
using WhereList = std::vector<WhereClause>;

struct ColumnDef {
    std::string name;
    DataType type;
};

// CREATE TABLE users (id INT, name TEXT)
struct CreateTableStatement {
    std::string table;
    std::vector<ColumnDef> columns;
};

// DROP TABLE users
struct DropTableStatement {
    std::string table;
};

// INSERT INTO users (id, name) VALUES (1, "Alice")
struct InsertStatement {
    std::string table;
    std::vector<std::string> columns;
    std::vector<std::string> values;  // parallel to columns
};

// SELECT * | col, col FROM users [WHERE cond AND cond ...]
struct SelectStatement {
    std::string table;
    std::vector<std::string> columns;  // empty means *
    WhereList where;
};

// UPDATE users SET col = literal, ... [WHERE cond AND cond ...]
struct UpdateStatement {
    std::string table;
    std::vector<std::pair<std::string, std::string>> assignments;  // col -> literal
    WhereList where;
};

// DELETE FROM users [WHERE cond AND cond ...]
struct DeleteStatement {
    std::string table;
    WhereList where;
};

using Statement =
    std::variant<CreateTableStatement, DropTableStatement, InsertStatement,
                 SelectStatement, UpdateStatement, DeleteStatement>;

}  // namespace cppdb
