#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "concurrency/RWLock.hpp"
#include "core/Table.hpp"
#include "memory/SharedPtr.hpp"
#include "structures/HashMap.hpp"

namespace cppdb {

// Top-level entry point: owns all tables by name.
//
// Thread safety: the table map is guarded by an RWLock. Tables are held by
// SharedPtr, and lookups return an owning copy — a table stays alive for any
// client still holding it even if another thread drops it, and the handle
// survives map rehashes (a Table itself is never moved once created; it
// contains a non-movable lock). Tables are internally synchronized, so
// mutating a table through the returned handle is safe.
class Database {
public:
    // Returns false if a table with that name already exists. Throws
    // std::invalid_argument on an empty name or a schema without an INT "id"
    // column (see Table).
    bool createTable(const std::string& name, Schema schema);

    // Empty SharedPtr if absent.
    SharedPtr<Table> getTable(const std::string& name) const;

    bool dropTable(const std::string& name);

    std::size_t tableCount() const;
    std::vector<std::string> tableNames() const;

private:
    HashMap<std::string, SharedPtr<Table>> tables_;
    mutable RWLock lock_;
};

}  // namespace cppdb
