#pragma once

#include <optional>
#include <string>
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

// SELECT * | col, col FROM users [WHERE ...]
struct SelectStatement {
    std::string table;
    std::vector<std::string> columns;  // empty means *
    std::optional<WhereClause> where;
};

// DELETE FROM users [WHERE ...]
struct DeleteStatement {
    std::string table;
    std::optional<WhereClause> where;
};

using Statement = std::variant<CreateTableStatement, DropTableStatement,
                               InsertStatement, SelectStatement, DeleteStatement>;

}  // namespace cppdb
