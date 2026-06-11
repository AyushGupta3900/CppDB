#include "core/Database.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace cppdb {

bool Database::createTable(const std::string& name, Schema schema) {
    if (name.empty()) {
        throw std::invalid_argument("Database: table name must not be empty");
    }
    if (tables_.contains(name)) return false;
    return tables_.insert(name, Table(std::move(schema)));
}

Table* Database::getTable(const std::string& name) noexcept {
    return tables_.find(name);
}

const Table* Database::getTable(const std::string& name) const noexcept {
    return tables_.find(name);
}

bool Database::dropTable(const std::string& name) { return tables_.erase(name); }

std::vector<std::string> Database::tableNames() const {
    std::vector<std::string> names;
    names.reserve(tables_.size());
    tables_.forEach([&](const std::string& name, const Table&) { names.push_back(name); });
    std::sort(names.begin(), names.end());
    return names;
}

}  // namespace cppdb
