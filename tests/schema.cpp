#include "core/Schema.hpp"

#include <stdexcept>

#include "test_framework.hpp"

using cppdb::Column;
using cppdb::DataType;
using cppdb::Schema;

int main() {
    // DataType <-> string round trip
    CHECK_EQ(cppdb::toString(DataType::INT), "INT");
    CHECK_EQ(cppdb::toString(DataType::TEXT), "TEXT");
    CHECK(cppdb::dataTypeFromString("INT") == DataType::INT);
    CHECK(cppdb::dataTypeFromString("TEXT") == DataType::TEXT);
    CHECK(!cppdb::dataTypeFromString("FLOAT").has_value());

    // Value validation
    CHECK(cppdb::isValidValueFor(DataType::INT, "42"));
    CHECK(cppdb::isValidValueFor(DataType::INT, "-7"));
    CHECK(!cppdb::isValidValueFor(DataType::INT, ""));
    CHECK(!cppdb::isValidValueFor(DataType::INT, "12abc"));
    CHECK(!cppdb::isValidValueFor(DataType::INT, "abc"));
    CHECK(!cppdb::isValidValueFor(DataType::INT, "99999999999999999999"));  // overflows int64
    CHECK(cppdb::isValidValueFor(DataType::TEXT, ""));
    CHECK(cppdb::isValidValueFor(DataType::TEXT, "hello world"));

    // Building a schema
    Schema schema;
    CHECK_EQ(schema.columnCount(), 0u);
    schema.addColumn("id", DataType::INT);
    schema.addColumn("name", DataType::TEXT);
    schema.addColumn("age", DataType::INT);
    CHECK_EQ(schema.columnCount(), 3u);

    // Lookup
    CHECK(schema.hasColumn("id"));
    CHECK(schema.hasColumn("age"));
    CHECK(!schema.hasColumn("email"));

    const Column* nameCol = schema.findColumn("name");
    CHECK(nameCol != nullptr);
    CHECK_EQ(nameCol->name, "name");
    CHECK(nameCol->type == DataType::TEXT);
    CHECK(schema.findColumn("missing") == nullptr);

    CHECK(schema.columnIndex("id") == 0u);
    CHECK(schema.columnIndex("age") == 2u);
    CHECK(!schema.columnIndex("missing").has_value());

    // Ordering is preserved
    CHECK_EQ(schema.columns()[0].name, "id");
    CHECK_EQ(schema.columns()[1].name, "name");
    CHECK_EQ(schema.columns()[2].name, "age");

    // Invalid additions
    CHECK_THROWS(schema.addColumn("id", DataType::TEXT), std::invalid_argument);
    CHECK_THROWS(schema.addColumn("", DataType::INT), std::invalid_argument);
    CHECK_EQ(schema.columnCount(), 3u);  // unchanged after failed adds

    return testfw::testSummary("schema");
}
