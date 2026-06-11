#include "core/Database.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace cppdb {

bool Database::createTable(const std::string& name, Schema schema) {
    if (name.empty()) {
        throw std::invalid_argument("Database: table name must not be empty");
    }
    // Construct (and let Table validate the schema) before locking.
    SharedPtr<Table> table(new Table(std::move(schema)));
    WriteGuard guard(lock_);
    return tables_.insert(name, std::move(table));
}

SharedPtr<Table> Database::getTable(const std::string& name) const {
    ReadGuard guard(lock_);
    const SharedPtr<Table>* table = tables_.find(name);
    return table == nullptr ? SharedPtr<Table>{} : *table;
}

bool Database::dropTable(const std::string& name) {
    WriteGuard guard(lock_);
    return tables_.erase(name);  // table dies with its last SharedPtr
}

std::size_t Database::tableCount() const {
    ReadGuard guard(lock_);
    return tables_.size();
}

std::vector<std::string> Database::tableNames() const {
    ReadGuard guard(lock_);
    std::vector<std::string> names;
    names.reserve(tables_.size());
    tables_.forEach(
        [&](const std::string& name, const SharedPtr<Table>&) { names.push_back(name); });
    std::sort(names.begin(), names.end());
    return names;
}

}  // namespace cppdb
