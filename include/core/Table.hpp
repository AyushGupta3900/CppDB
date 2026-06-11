#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "concurrency/RWLock.hpp"
#include "core/Row.hpp"
#include "core/Schema.hpp"
#include "structures/BTree.hpp"

namespace cppdb {

// A table owns its schema and its rows, stored in a B-tree clustered on the
// INT primary-key column "id" — point lookups are O(log n), scans come back
// in ascending id order, and id ranges are answered without a full scan.
// Lookups return copies so callers never hold pointers into storage (the
// tree rebalances, and other threads may write).
//
// Thread safety: every public method takes the table's RWLock — reads run in
// parallel, writes are exclusive. The schema is immutable after construction
// and needs no lock. Holding a Table reference across calls is safe; the
// individual calls are atomic, sequences of calls are not.
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

    std::size_t rowCount() const;

    // Scan queries; rows are visited in ascending id order.
    std::vector<Row> selectWhere(const std::function<bool(const Row&)>& predicate) const;
    std::vector<Row> selectAll() const;
    std::size_t deleteWhere(const std::function<bool(const Row&)>& predicate);

    // All rows with lo <= id <= hi via a pruned B-tree range scan.
    std::vector<Row> selectIdRange(std::int64_t lo, std::int64_t hi) const;

private:
    std::shared_ptr<const Schema> schema_;
    BTree<std::int64_t, Row> rows_;
    mutable RWLock lock_;  // mutable: reads lock too, even on const methods
};

}  // namespace cppdb
