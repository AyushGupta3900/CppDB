#include "core/Schema.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <stdexcept>

#include "core/Row.hpp"

namespace cppdb {

std::string toString(DataType type) {
    switch (type) {
        case DataType::INT:
            return "INT";
        case DataType::TEXT:
            return "TEXT";
    }
    return "UNKNOWN";
}

std::optional<DataType> dataTypeFromString(std::string_view name) {
    if (name == "INT") return DataType::INT;
    if (name == "TEXT") return DataType::TEXT;
    return std::nullopt;
}

bool isValidValueFor(DataType type, const std::string& value) {
    switch (type) {
        case DataType::INT: {
            std::int64_t parsed = 0;
            const char* first = value.data();
            const char* last = first + value.size();
            auto [ptr, ec] = std::from_chars(first, last, parsed);
            return ec == std::errc() && ptr == last;
        }
        case DataType::TEXT:
            return true;
    }
    return false;
}

void Schema::addColumn(const std::string& name, DataType type) {
    if (name.empty()) {
        throw std::invalid_argument("Schema: column name must not be empty");
    }
    if (hasColumn(name)) {
        throw std::invalid_argument("Schema: duplicate column '" + name + "'");
    }
    columns_.push_back(Column{name, type});
}

bool Schema::hasColumn(const std::string& name) const noexcept {
    return findColumn(name) != nullptr;
}

const Column* Schema::findColumn(const std::string& name) const noexcept {
    auto it = std::find_if(columns_.begin(), columns_.end(),
                           [&](const Column& c) { return c.name == name; });
    return it == columns_.end() ? nullptr : &*it;
}

std::optional<std::size_t> Schema::columnIndex(const std::string& name) const noexcept {
    for (std::size_t i = 0; i < columns_.size(); ++i) {
        if (columns_[i].name == name) return i;
    }
    return std::nullopt;
}

bool Schema::validate(const Row& row) const {
    for (const Column& column : columns_) {
        if (!row.has(column.name)) return false;
        if (!isValidValueFor(column.type, row.get(column.name))) return false;
    }
    return true;
}

}  // namespace cppdb
