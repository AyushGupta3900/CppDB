#include "core/Table.hpp"

#include <stdexcept>
#include <utility>

namespace cppdb {

Table::Table(Schema schema, std::size_t cacheCapacity)
    : schema_(std::make_shared<const Schema>(std::move(schema))),
      cache_(cacheCapacity) {
    const Column* idColumn = schema_->findColumn(kIdColumn);
    if (idColumn == nullptr || idColumn->type != DataType::INT) {
        throw std::invalid_argument(
            "Table: schema requires an INT primary-key column named 'id'");
    }
}

Row Table::makeRow() const { return Row(schema_); }

bool Table::insert(Row row) {
    // Validate before taking the write lock: schema is immutable and the row
    // is caller-local, so this needs no synchronization.
    if (row.schema().columns() != schema_->columns()) {
        throw std::invalid_argument("Table: row schema does not match table schema");
    }
    if (!schema_->validate(row)) {
        throw std::invalid_argument("Table: row is incomplete or fails type checks");
    }
    const std::int64_t id = row.getInt(kIdColumn);
    WriteGuard guard(lock_);
    return rows_.insert(id, std::move(row));
}

std::optional<Row> Table::findById(std::int64_t id) const {
    ReadGuard guard(lock_);
    {
        std::lock_guard<std::mutex> cacheGuard(cacheMutex_);
        if (const Row* hit = cache_.get(id)) return *hit;
    }
    const Row* row = rows_.search(id);
    if (row == nullptr) return std::nullopt;
    {
        std::lock_guard<std::mutex> cacheGuard(cacheMutex_);
        cache_.put(id, *row);
    }
    return *row;
}

bool Table::deleteById(std::int64_t id) {
    WriteGuard guard(lock_);
    const bool removed = rows_.remove(id);
    if (removed) {
        std::lock_guard<std::mutex> cacheGuard(cacheMutex_);
        cache_.erase(id);
    }
    return removed;
}

std::size_t Table::rowCount() const {
    ReadGuard guard(lock_);
    return rows_.size();
}

std::vector<Row> Table::selectWhere(
    const std::function<bool(const Row&)>& predicate) const {
    ReadGuard guard(lock_);
    std::vector<Row> result;
    rows_.forEachInOrder([&](const std::int64_t&, const Row& row) {
        if (predicate(row)) result.push_back(row);
    });
    return result;
}

std::vector<Row> Table::selectAll() const {
    return selectWhere([](const Row&) { return true; });
}

std::size_t Table::deleteWhere(const std::function<bool(const Row&)>& predicate) {
    WriteGuard guard(lock_);  // scan + erase as one atomic operation
    std::vector<std::int64_t> doomed;
    rows_.forEachInOrder([&](const std::int64_t& id, const Row& row) {
        if (predicate(row)) doomed.push_back(id);
    });
    std::lock_guard<std::mutex> cacheGuard(cacheMutex_);
    for (const std::int64_t id : doomed) {
        rows_.remove(id);
        cache_.erase(id);
    }
    return doomed.size();
}

std::size_t Table::cacheHits() const {
    std::lock_guard<std::mutex> cacheGuard(cacheMutex_);
    return cache_.hits();
}

std::size_t Table::cacheMisses() const {
    std::lock_guard<std::mutex> cacheGuard(cacheMutex_);
    return cache_.misses();
}

std::vector<Row> Table::selectIdRange(std::int64_t lo, std::int64_t hi) const {
    ReadGuard guard(lock_);
    std::vector<Row> result;
    rows_.forEachRange(lo, hi,
                       [&](const std::int64_t&, const Row& row) { result.push_back(row); });
    return result;
}

}  // namespace cppdb
