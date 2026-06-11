#include "core/Row.hpp"

#include <memory>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "core/Schema.hpp"
#include "test_framework.hpp"

using cppdb::DataType;
using cppdb::Row;
using cppdb::Schema;

namespace {

std::shared_ptr<const Schema> makeUserSchema() {
    auto schema = std::make_shared<Schema>();
    schema->addColumn("id", DataType::INT);
    schema->addColumn("name", DataType::TEXT);
    schema->addColumn("age", DataType::INT);
    return schema;
}

}  // namespace

int main() {
    auto schema = makeUserSchema();

    // Construction and set/get
    CHECK_THROWS(Row(nullptr), std::invalid_argument);
    Row row(schema);
    CHECK(!row.has("id"));
    CHECK(!row.isComplete());

    row.set("id", "1");
    row.set("name", "Alice");
    CHECK(row.has("id"));
    CHECK(!row.isComplete());  // age still unset
    row.set("age", "30");
    CHECK(row.isComplete());

    CHECK_EQ(row.get("name"), "Alice");
    CHECK_EQ(row.getInt("id"), 1);
    CHECK_EQ(row.getInt("age"), 30);

    // Type and column errors
    CHECK_THROWS(row.set("age", "not-a-number"), std::invalid_argument);
    CHECK_THROWS(row.set("email", "x@y.z"), std::invalid_argument);
    CHECK_THROWS(row.get("email"), std::out_of_range);
    CHECK_THROWS(row.getInt("name"), std::invalid_argument);
    Row blank(schema);
    CHECK_THROWS(blank.get("id"), std::out_of_range);

    // Overwrite keeps last value
    row.set("name", "Bob");
    CHECK_EQ(row.get("name"), "Bob");
    row.set("name", "Alice");

    // Schema::validate
    CHECK(schema->validate(row));
    CHECK(!schema->validate(blank));

    // Copy constructor: deep, independent copy
    Row copy(row);
    CHECK(copy == row);
    copy.set("name", "Carol");
    CHECK_EQ(copy.get("name"), "Carol");
    CHECK_EQ(row.get("name"), "Alice");  // original untouched
    CHECK(!(copy == row));

    // Copy assignment
    Row assigned(schema);
    assigned = row;
    CHECK(assigned == row);

    // Move constructor: steals state, original row preserved in the new object
    Row moved(std::move(assigned));
    CHECK(moved == row);

    // Move assignment
    Row moveTarget(schema);
    moveTarget = std::move(moved);
    CHECK(moveTarget == row);

    // Self-assignment is a no-op
    Row& self = moveTarget;
    moveTarget = self;
    CHECK(moveTarget == row);

    // operator<< prints schema order with NULL for unset
    Row printable(schema);
    printable.set("id", "7");
    printable.set("name", "Dave");
    std::ostringstream oss;
    oss << printable;
    CHECK_EQ(oss.str(), "id=7, name=Dave, age=NULL");

    // Rows share the schema, they do not copy it
    CHECK(&copy.schema() == schema.get());

    // Cells are typed: INT columns hold a native int64, parsed once in set()
    Row typed(schema);
    typed.set("id", "007");
    CHECK_EQ(typed.get("id"), "7");  // canonical integer, not the raw text
    CHECK(std::holds_alternative<std::int64_t>(typed.value("id")));
    typed.set("name", "42");
    CHECK(std::holds_alternative<std::string>(typed.value("name")));
    CHECK_THROWS(typed.value("age"), std::out_of_range);  // unset

    return testfw::testSummary("row");
}
