#include "query/QueryExecutor.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <functional>
#include <limits>
#include <sstream>
#include <utility>
#include <variant>
#include <vector>

#include "query/Lexer.hpp"
#include "query/Parser.hpp"

namespace cppdb {

namespace {

std::int64_t parseInt(const std::string& raw) {
    std::int64_t value = 0;
    std::from_chars(raw.data(), raw.data() + raw.size(), value);
    return value;  // callers validate with isValidValueFor first
}

bool applyOp(CompareOp op, int cmp) {
    switch (op) {
        case CompareOp::EQ: return cmp == 0;
        case CompareOp::NE: return cmp != 0;
        case CompareOp::LT: return cmp < 0;
        case CompareOp::LE: return cmp <= 0;
        case CompareOp::GT: return cmp > 0;
        case CompareOp::GE: return cmp >= 0;
    }
    return false;
}

// Compiles a WHERE clause into a row predicate, validating the column and
// the literal's type up front.
std::function<bool(const Row&)> makePredicate(const Schema& schema,
                                              const WhereClause& where) {
    const Column* column = schema.findColumn(where.column);
    if (column == nullptr) {
        throw QueryError("no such column '" + where.column + "'");
    }
    if (!isValidValueFor(column->type, where.value)) {
        throw QueryError("column '" + where.column + "' is " +
                         toString(column->type) + ", got '" + where.value + "'");
    }
    if (column->type == DataType::INT) {
        // Parse the literal once; rows already hold native int64 cells.
        const std::int64_t expected = parseInt(where.value);
        return [name = where.column, expected, op = where.op](const Row& row) {
            const std::int64_t actual = row.getInt(name);
            return applyOp(op, actual < expected ? -1 : (actual > expected ? 1 : 0));
        };
    }
    return [name = where.column, literal = where.value, op = where.op](const Row& row) {
        const int raw = row.get(name).compare(literal);
        return applyOp(op, raw < 0 ? -1 : (raw > 0 ? 1 : 0));
    };
}

// AND-combines every clause into one predicate (validates all of them).
std::function<bool(const Row&)> makeConjunction(const Schema& schema,
                                                const WhereList& conditions) {
    std::vector<std::function<bool(const Row&)>> predicates;
    predicates.reserve(conditions.size());
    for (const WhereClause& clause : conditions) {
        predicates.push_back(makePredicate(schema, clause));
    }
    return [predicates = std::move(predicates)](const Row& row) {
        for (const auto& predicate : predicates) {
            if (!predicate(row)) return false;
        }
        return true;
    };
}

bool isIndexableIdClause(const WhereClause& clause) {
    return clause.column == Table::kIdColumn && clause.op != CompareOp::NE;
}

// Rows matching an indexable id clause, straight off the B-tree: a point
// lookup for `=`, a pruned range scan for `< <= > >=`.
std::vector<Row> rowsByIdClause(const Table& table, const WhereClause& clause) {
    if (!isValidValueFor(DataType::INT, clause.value)) {
        throw QueryError("column 'id' is INT, got '" + clause.value + "'");
    }
    const std::int64_t v = parseInt(clause.value);
    constexpr std::int64_t kMin = std::numeric_limits<std::int64_t>::min();
    constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
    switch (clause.op) {
        case CompareOp::EQ: {
            auto row = table.findById(v);
            return row ? std::vector<Row>{std::move(*row)} : std::vector<Row>{};
        }
        case CompareOp::LT:
            return v == kMin ? std::vector<Row>{} : table.selectIdRange(kMin, v - 1);
        case CompareOp::LE:
            return table.selectIdRange(kMin, v);
        case CompareOp::GT:
            return v == kMax ? std::vector<Row>{} : table.selectIdRange(v + 1, kMax);
        case CompareOp::GE:
            return table.selectIdRange(v, kMax);
        case CompareOp::NE:
            break;  // not indexable; callers exclude this
    }
    throw QueryError("internal: unindexable id clause");
}

// Fetch rows for a SELECT, narrowing through the B-tree when any clause is
// an indexable primary-key comparison, then filtering with the rest.
std::vector<Row> fetchRows(const Table& table, const WhereList& where) {
    if (where.empty()) return table.selectAll();

    const auto indexable =
        std::find_if(where.begin(), where.end(), isIndexableIdClause);
    if (indexable == where.end()) {
        return table.selectWhere(makeConjunction(table.schema(), where));
    }

    std::vector<Row> candidates = rowsByIdClause(table, *indexable);
    WhereList rest;
    for (auto it = where.begin(); it != where.end(); ++it) {
        if (it != indexable) rest.push_back(*it);
    }
    if (rest.empty()) return candidates;

    const auto predicate = makeConjunction(table.schema(), rest);
    std::vector<Row> result;
    for (Row& row : candidates) {
        if (predicate(row)) result.push_back(std::move(row));
    }
    return result;
}

std::string formatRow(const Row& row, const std::vector<std::string>& columns) {
    if (columns.empty()) {  // SELECT * — Row prints itself in schema order
        std::ostringstream oss;
        oss << row;
        return oss.str();
    }
    std::ostringstream oss;
    for (std::size_t i = 0; i < columns.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << columns[i] << "=" << row.get(columns[i]);
    }
    return oss.str();
}

std::string rowsSuffix(std::size_t n) {
    return std::to_string(n) + (n == 1 ? " row" : " rows");
}

}  // namespace

std::string QueryExecutor::execute(const std::string& query) {
    try {
        Lexer lexer(query);
        Parser parser(lexer.tokenize());
        const Statement statement = parser.parse();
        return std::visit([this](const auto& stmt) { return run(stmt); }, statement);
    } catch (const QueryError& e) {
        return std::string("ERROR: ") + e.what();
    } catch (const std::exception& e) {
        return std::string("ERROR: ") + e.what();
    }
}

SharedPtr<Table> QueryExecutor::tableOrThrow(const std::string& name) const {
    SharedPtr<Table> table = db_.getTable(name);
    if (!table) throw QueryError("no such table '" + name + "'");
    return table;
}

std::string QueryExecutor::run(const CreateTableStatement& stmt) {
    Schema schema;
    for (const ColumnDef& def : stmt.columns) {
        schema.addColumn(def.name, def.type);  // throws on duplicates
    }
    // Table's constructor enforces the INT id primary key and reports it
    // better than we could here.
    if (!db_.createTable(stmt.table, std::move(schema))) {
        throw QueryError("table '" + stmt.table + "' already exists");
    }
    return "OK";
}

std::string QueryExecutor::run(const DropTableStatement& stmt) {
    if (!db_.dropTable(stmt.table)) {
        throw QueryError("no such table '" + stmt.table + "'");
    }
    return "OK";
}

std::string QueryExecutor::run(const InsertStatement& stmt) {
    SharedPtr<Table> table = tableOrThrow(stmt.table);
    Row row = table->makeRow();
    for (std::size_t i = 0; i < stmt.columns.size(); ++i) {
        row.set(stmt.columns[i], stmt.values[i]);  // throws on bad column/type
    }
    const std::int64_t id = row.getInt(Table::kIdColumn);  // throws if id unset
    if (!table->insert(std::move(row))) {
        throw QueryError("duplicate id " + std::to_string(id) + " in table '" +
                         stmt.table + "'");
    }
    return "OK";
}

std::string QueryExecutor::run(const SelectStatement& stmt) {
    SharedPtr<Table> table = tableOrThrow(stmt.table);
    for (const std::string& column : stmt.columns) {
        if (!table->schema().hasColumn(column)) {
            throw QueryError("no such column '" + column + "'");
        }
    }
    const std::vector<Row> rows = fetchRows(*table, stmt.where);
    if (rows.empty()) return "(0 rows)";
    std::ostringstream oss;
    for (const Row& row : rows) {
        oss << formatRow(row, stmt.columns) << '\n';
    }
    oss << "(" << rowsSuffix(rows.size()) << ")";
    return oss.str();
}

std::string QueryExecutor::run(const UpdateStatement& stmt) {
    SharedPtr<Table> table = tableOrThrow(stmt.table);
    const auto predicate =
        stmt.where.empty()
            ? std::function<bool(const Row&)>([](const Row&) { return true; })
            : makeConjunction(table->schema(), stmt.where);
    std::size_t updated = 0;
    try {
        updated = table->updateWhere(predicate, stmt.assignments);
    } catch (const std::invalid_argument& e) {
        throw QueryError(e.what());  // bad column / type / primary-key update
    }
    return "OK (" + rowsSuffix(updated) + " updated)";
}

std::string QueryExecutor::run(const DeleteStatement& stmt) {
    SharedPtr<Table> table = tableOrThrow(stmt.table);
    std::size_t deleted = 0;
    if (stmt.where.size() == 1 && stmt.where[0].column == Table::kIdColumn &&
        stmt.where[0].op == CompareOp::EQ &&
        isValidValueFor(DataType::INT, stmt.where[0].value)) {
        deleted = table->deleteById(parseInt(stmt.where[0].value)) ? 1 : 0;
    } else if (!stmt.where.empty()) {
        deleted = table->deleteWhere(makeConjunction(table->schema(), stmt.where));
    } else {
        deleted = table->deleteWhere([](const Row&) { return true; });
    }
    return "OK (" + rowsSuffix(deleted) + " deleted)";
}

}  // namespace cppdb
