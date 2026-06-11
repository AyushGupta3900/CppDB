#include "core/Row.hpp"

#include <charconv>
#include <stdexcept>
#include <utility>

namespace cppdb {

Row::Row(std::shared_ptr<const Schema> schema) : schema_(std::move(schema)) {
    if (!schema_) {
        throw std::invalid_argument("Row: schema must not be null");
    }
    values_.resize(schema_->columnCount());
}

Row::Row(const Row& other) : schema_(other.schema_), values_(other.values_) {}

Row::Row(Row&& other) noexcept
    : schema_(std::move(other.schema_)), values_(std::move(other.values_)) {}

Row& Row::operator=(const Row& other) {
    if (this != &other) {
        schema_ = other.schema_;
        values_ = other.values_;
    }
    return *this;
}

Row& Row::operator=(Row&& other) noexcept {
    if (this != &other) {
        schema_ = std::move(other.schema_);
        values_ = std::move(other.values_);
    }
    return *this;
}

Row::~Row() = default;

void Row::set(const std::string& column, std::string value) {
    const auto index = schema_->columnIndex(column);
    if (!index) {
        throw std::invalid_argument("Row: unknown column '" + column + "'");
    }
    const DataType type = schema_->columns()[*index].type;
    if (!isValidValueFor(type, value)) {
        throw std::invalid_argument("Row: value '" + value + "' is not a valid " +
                                    toString(type) + " for column '" + column + "'");
    }
    if (*index >= values_.size()) {
        values_.resize(schema_->columnCount());  // schema grew after construction
    }
    values_[*index] = std::move(value);
}

const std::string& Row::get(const std::string& column) const {
    const auto index = schema_->columnIndex(column);
    if (!index || *index >= values_.size() || !values_[*index]) {
        throw std::out_of_range("Row: column '" + column + "' is not set");
    }
    return *values_[*index];
}

std::int64_t Row::getInt(const std::string& column) const {
    const Column* col = schema_->findColumn(column);
    if (col && col->type != DataType::INT) {
        throw std::invalid_argument("Row: column '" + column + "' is not INT");
    }
    const std::string& raw = get(column);
    std::int64_t parsed = 0;
    std::from_chars(raw.data(), raw.data() + raw.size(), parsed);
    return parsed;  // set() already guaranteed this parses
}

bool Row::has(const std::string& column) const noexcept {
    const auto index = schema_->columnIndex(column);
    return index && *index < values_.size() && values_[*index].has_value();
}

bool Row::isComplete() const noexcept {
    if (values_.size() != schema_->columnCount()) return false;
    for (const auto& value : values_) {
        if (!value) return false;
    }
    return true;
}

bool operator==(const Row& lhs, const Row& rhs) {
    return lhs.schema_->columns() == rhs.schema_->columns() &&
           lhs.values_ == rhs.values_;
}

std::ostream& operator<<(std::ostream& os, const Row& row) {
    const auto& columns = row.schema_->columns();
    for (std::size_t i = 0; i < columns.size(); ++i) {
        if (i > 0) os << ", ";
        os << columns[i].name << "=";
        if (i < row.values_.size() && row.values_[i]) {
            os << *row.values_[i];
        } else {
            os << "NULL";
        }
    }
    return os;
}

}  // namespace cppdb
