#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "core/Table.hpp"
#include "structures/HashMap.hpp"

namespace cppdb {

// Top-level entry point: owns all tables by name.
class Database {
public:
    // Returns false if a table with that name already exists. Throws
    // std::invalid_argument on an empty name or a schema without an INT "id"
    // column (see Table).
    bool createTable(const std::string& name, Schema schema);

    // nullptr if absent. NOTE: the pointer is invalidated by the next
    // createTable/dropTable (the table map may rehash) — do not store it.
    Table* getTable(const std::string& name) noexcept;
    const Table* getTable(const std::string& name) const noexcept;

    bool dropTable(const std::string& name);

    std::size_t tableCount() const noexcept { return tables_.size(); }
    std::vector<std::string> tableNames() const;

private:
    HashMap<std::string, Table> tables_;
};

}  // namespace cppdb
