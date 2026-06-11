#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "core/Schema.hpp"

namespace cppdb {

// A single record bound to a schema. Values are stored as strings in schema
// column order; a column is NULL until set(). The schema is shared (not
// referenced) so a Row stays valid even if the owning table is moved.
//
// The Rule of Five is implemented explicitly: this class is the project's
// exercise in copy/move semantics, see concepts/ for the why. A moved-from
// Row may only be destroyed or assigned to.
class Row {
public:
    // Throws std::invalid_argument if schema is null.
    explicit Row(std::shared_ptr<const Schema> schema);

    Row(const Row& other);
    Row(Row&& other) noexcept;
    Row& operator=(const Row& other);
    Row& operator=(Row&& other) noexcept;
    ~Row();

    // Throws std::invalid_argument on unknown column or a value that does not
    // type-check against the column's DataType.
    void set(const std::string& column, std::string value);

    // Throws std::out_of_range on unknown or unset column.
    const std::string& get(const std::string& column) const;

    // get() parsed as int64. Throws std::out_of_range like get(); throws
    // std::invalid_argument if the column is not an INT column.
    std::int64_t getInt(const std::string& column) const;

    bool has(const std::string& column) const noexcept;
    bool isComplete() const noexcept;  // every schema column has a value

    const Schema& schema() const noexcept { return *schema_; }
    std::shared_ptr<const Schema> schemaPtr() const noexcept { return schema_; }

    // Structural equality: same columns (name + type) and same values.
    friend bool operator==(const Row& lhs, const Row& rhs);

    // Prints "id=1, name=Alice, age=NULL" in schema order.
    friend std::ostream& operator<<(std::ostream& os, const Row& row);

private:
    std::shared_ptr<const Schema> schema_;
    std::vector<std::optional<std::string>> values_;  // parallel to schema columns
};

}  // namespace cppdb
