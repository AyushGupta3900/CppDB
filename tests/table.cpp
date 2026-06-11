#include "core/Table.hpp"

#include <stdexcept>
#include <string>

#include "core/Database.hpp"
#include "core/Schema.hpp"
#include "test_framework.hpp"

using cppdb::Database;
using cppdb::DataType;
using cppdb::Row;
using cppdb::Schema;
using cppdb::Table;

namespace {

Schema userSchema() {
    Schema schema;
    schema.addColumn("id", DataType::INT);
    schema.addColumn("name", DataType::TEXT);
    schema.addColumn("age", DataType::INT);
    return schema;
}

Row makeUser(const Table& table, std::int64_t id, const std::string& name, int age) {
    Row row = table.makeRow();
    row.set("id", std::to_string(id));
    row.set("name", name);
    row.set("age", std::to_string(age));
    return row;
}

}  // namespace

int main() {
    // Schema must carry an INT "id" primary key
    Schema noId;
    noId.addColumn("name", DataType::TEXT);
    CHECK_THROWS(Table(noId), std::invalid_argument);
    Schema textId;
    textId.addColumn("id", DataType::TEXT);
    CHECK_THROWS(Table(textId), std::invalid_argument);

    Table users{userSchema()};
    CHECK_EQ(users.rowCount(), 0u);

    // Insert + findById round trip
    CHECK(users.insert(makeUser(users, 1, "Alice", 30)));
    CHECK(users.insert(makeUser(users, 2, "Bob", 25)));
    CHECK_EQ(users.rowCount(), 2u);

    auto alice = users.findById(1);
    CHECK(alice.has_value());
    CHECK_EQ(alice->get("name"), "Alice");
    CHECK(!users.findById(404).has_value());

    // Duplicate primary key refused, table unchanged
    CHECK(!users.insert(makeUser(users, 1, "Imposter", 99)));
    CHECK_EQ(users.findById(1)->get("name"), "Alice");

    // Incomplete row refused
    Row partial = users.makeRow();
    partial.set("id", "3");
    CHECK_THROWS(users.insert(partial), std::invalid_argument);

    // Row from a different schema refused
    Schema otherSchema;
    otherSchema.addColumn("id", DataType::INT);
    Table other{otherSchema};
    Row foreign = other.makeRow();
    foreign.set("id", "5");
    CHECK_THROWS(users.insert(foreign), std::invalid_argument);
    CHECK_EQ(users.rowCount(), 2u);

    // findById returns a copy: mutating it does not touch storage
    auto copy = users.findById(2);
    copy->set("name", "Changed");
    CHECK_EQ(users.findById(2)->get("name"), "Bob");

    // selectWhere / selectAll
    CHECK(users.insert(makeUser(users, 3, "Carol", 35)));
    auto adults = users.selectWhere([](const Row& r) { return r.getInt("age") >= 30; });
    CHECK_EQ(adults.size(), 2u);
    CHECK_EQ(users.selectAll().size(), 3u);

    // deleteById / deleteWhere
    CHECK(users.deleteById(2));
    CHECK(!users.deleteById(2));
    CHECK_EQ(users.rowCount(), 2u);
    CHECK_EQ(users.deleteWhere([](const Row& r) { return r.getInt("age") > 100; }), 0u);
    CHECK_EQ(users.deleteWhere([](const Row& r) { return r.getInt("age") >= 30; }), 2u);
    CHECK_EQ(users.rowCount(), 0u);

    // Volume: storage survives many node splits
    Table bulk{userSchema()};
    for (int i = 0; i < 1000; ++i) {
        CHECK(bulk.insert(makeUser(bulk, i, "user" + std::to_string(i), i % 80)));
    }
    CHECK_EQ(bulk.rowCount(), 1000u);
    CHECK_EQ(bulk.findById(999)->get("name"), "user999");

    // B-tree storage: scans come back ordered by id even after shuffled inserts
    Table ordered{userSchema()};
    for (int id : {42, 7, 99, 1, 63}) {
        CHECK(ordered.insert(makeUser(ordered, id, "u", 20)));
    }
    auto scanned = ordered.selectAll();
    CHECK_EQ(scanned.size(), 5u);
    CHECK_EQ(scanned[0].getInt("id"), 1);
    CHECK_EQ(scanned[4].getInt("id"), 99);

    // Range scan is inclusive on both ends
    auto ranged = ordered.selectIdRange(7, 63);
    CHECK_EQ(ranged.size(), 3u);
    CHECK_EQ(ranged[0].getInt("id"), 7);
    CHECK_EQ(ranged[2].getInt("id"), 63);

    // ---- LRU read cache ----
    Table cached{userSchema(), /*cacheCapacity=*/4};
    cached.insert(makeUser(cached, 1, "Alice", 30));
    CHECK_EQ(cached.cacheHits(), 0u);
    CHECK(cached.findById(1).has_value());   // miss -> fills cache
    CHECK(cached.findById(1).has_value());   // hit
    CHECK_EQ(cached.cacheHits(), 1u);
    CHECK_EQ(cached.cacheMisses(), 1u);

    // Deletion invalidates: a reinserted id must serve the NEW row
    CHECK(cached.deleteById(1));
    CHECK(!cached.findById(1).has_value());
    cached.insert(makeUser(cached, 1, "NewAlice", 31));
    CHECK_EQ(cached.findById(1)->get("name"), "NewAlice");

    // deleteWhere also invalidates cached rows
    cached.insert(makeUser(cached, 2, "Bob", 25));
    CHECK(cached.findById(2).has_value());
    CHECK_EQ(cached.deleteWhere([](const Row& r) { return r.getInt("id") == 2; }), 1u);
    CHECK(!cached.findById(2).has_value());

    // ---- Database ----
    Database db;
    CHECK(db.createTable("users", userSchema()));
    CHECK(!db.createTable("users", userSchema()));  // duplicate name
    CHECK_THROWS(db.createTable("", userSchema()), std::invalid_argument);
    CHECK_THROWS(db.createTable("bad", noId), std::invalid_argument);
    CHECK_EQ(db.tableCount(), 1u);

    cppdb::SharedPtr<Table> usersTable = db.getTable("users");
    CHECK(static_cast<bool>(usersTable));
    CHECK(!db.getTable("missing"));

    // Mutations through getTable persist
    usersTable->insert(makeUser(*usersTable, 1, "Alice", 30));
    CHECK_EQ(db.getTable("users")->rowCount(), 1u);

    // A held handle keeps the table alive across drop + recreate
    CHECK(db.dropTable("users"));
    CHECK_EQ(usersTable->rowCount(), 1u);  // still alive through our SharedPtr
    CHECK(db.createTable("users", userSchema()));
    CHECK_EQ(db.getTable("users")->rowCount(), 0u);  // fresh table
    usersTable.reset();

    CHECK(db.createTable("orders", userSchema()));
    auto names = db.tableNames();
    CHECK_EQ(names.size(), 2u);
    CHECK_EQ(names[0], "orders");
    CHECK_EQ(names[1], "users");

    CHECK(db.dropTable("orders"));
    CHECK(!db.dropTable("orders"));
    CHECK_EQ(db.tableCount(), 1u);

    return testfw::testSummary("table");
}
