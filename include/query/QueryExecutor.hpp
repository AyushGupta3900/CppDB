#pragma once

#include <string>

#include "core/Database.hpp"
#include "query/Ast.hpp"

namespace cppdb {

// Runs raw query strings against a Database: lex -> parse -> dispatch on the
// AST. Never throws — every failure (syntax, unknown table/column, type
// mismatch, duplicate id) comes back as an "ERROR: ..." string, because the
// caller may be a network server that must keep running.
//
// WHERE clauses on the primary key use the B-tree instead of scanning:
// `id = n` is a point lookup, and `id < n`, `id <= n`, `id > n`, `id >= n`
// are pruned range scans.
//
// Thread safe: it holds only a Database reference, all state is per-call,
// and Table/Database are internally synchronized.
class QueryExecutor {
public:
    explicit QueryExecutor(Database& db) : db_(db) {}

    std::string execute(const std::string& query);

private:
    std::string run(const CreateTableStatement& stmt);
    std::string run(const DropTableStatement& stmt);
    std::string run(const InsertStatement& stmt);
    std::string run(const SelectStatement& stmt);
    std::string run(const UpdateStatement& stmt);
    std::string run(const DeleteStatement& stmt);

    SharedPtr<Table> tableOrThrow(const std::string& name) const;

    Database& db_;
};

}  // namespace cppdb
