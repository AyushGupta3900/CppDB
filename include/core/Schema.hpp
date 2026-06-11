#pragma once

#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace cppdb {

enum class DataType {
    INT,
    TEXT,
};

// A typed cell: INT columns hold a real int64, TEXT columns hold a string.
// Parsing happens once at the boundary (Row::set); everything downstream
// compares and copies native values instead of re-parsing strings.
using Value = std::variant<std::int64_t, std::string>;

std::string toString(DataType type);
std::string toString(const Value& value);

// Returns nullopt for unknown names. Expects the SQL spelling ("INT", "TEXT").
std::optional<DataType> dataTypeFromString(std::string_view name);

// True if `value` is a legal literal for `type` (INT must parse as a 64-bit
// signed integer; TEXT accepts anything).
bool isValidValueFor(DataType type, const std::string& value);

struct Column {
    std::string name;
    DataType type;

    bool operator==(const Column&) const = default;
};

class Row;

// Ordered set of column definitions for one table. Column names are unique.
class Schema {
public:
    // Throws std::invalid_argument on an empty or duplicate column name.
    void addColumn(const std::string& name, DataType type);

    const std::vector<Column>& columns() const noexcept { return columns_; }
    std::size_t columnCount() const noexcept { return columns_.size(); }

    bool hasColumn(const std::string& name) const noexcept;
    const Column* findColumn(const std::string& name) const noexcept;
    std::optional<std::size_t> columnIndex(const std::string& name) const noexcept;

    // True if every column of this schema is set on `row` with a value that
    // type-checks. Defined alongside Row.
    bool validate(const Row& row) const;

private:
    std::vector<Column> columns_;
};

}  // namespace cppdb
