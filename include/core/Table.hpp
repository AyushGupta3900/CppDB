#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "core/Row.hpp"
#include "core/Schema.hpp"
#include "structures/HashMap.hpp"

namespace cppdb {

// A table owns its schema and its rows, keyed by the INT primary-key column
// "id". Lookups return copies so callers never hold pointers into storage
// (the storage container may rehash, and later stages add concurrent access).
class Table {
public:
    static constexpr const char* kIdColumn = "id";

    // Throws std::invalid_argument unless the schema has an INT "id" column.
    explicit Table(Schema schema);

    const Schema& schema() const noexcept { return *schema_; }

    // A blank row bound to this table's schema.
    Row makeRow() const;

    // Throws std::invalid_argument if the row's schema differs from the
    // table's or the row is incomplete/invalid. Returns false on duplicate id.
    bool insert(Row row);

    std::optional<Row> findById(std::int64_t id) const;
    bool deleteById(std::int64_t id);

    std::size_t rowCount() const noexcept;

    // Full-scan queries; the predicate sees each row in unspecified order.
    std::vector<Row> selectWhere(const std::function<bool(const Row&)>& predicate) const;
    std::vector<Row> selectAll() const;
    std::size_t deleteWhere(const std::function<bool(const Row&)>& predicate);

private:
    std::shared_ptr<const Schema> schema_;
    HashMap<std::int64_t, Row> rows_;
};

}  // namespace cppdb
