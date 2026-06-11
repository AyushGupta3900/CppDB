// Hammers a Table and a Database from many threads at once. The assertions
// check totals; ThreadSanitizer (make tsan target or manual build) checks the
// absence of data races.

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "core/Database.hpp"
#include "core/Table.hpp"
#include "test_framework.hpp"

using cppdb::Database;
using cppdb::DataType;
using cppdb::Row;
using cppdb::Schema;
using cppdb::SharedPtr;
using cppdb::Table;

namespace {

Schema userSchema() {
    Schema schema;
    schema.addColumn("id", DataType::INT);
    schema.addColumn("name", DataType::TEXT);
    return schema;
}

void insertUser(Table& table, std::int64_t id) {
    Row row = table.makeRow();
    row.set("id", std::to_string(id));
    row.set("name", "user" + std::to_string(id));
    table.insert(std::move(row));
}

}  // namespace

static void testConcurrentTable() {
    Table table{userSchema()};
    const int writerThreads = 4;
    const int rowsPerWriter = 2'000;
    std::atomic<bool> readMismatch{false};

    std::vector<std::thread> threads;
    // Writers insert disjoint id ranges.
    for (int w = 0; w < writerThreads; ++w) {
        threads.emplace_back([&table, w] {
            const std::int64_t base = static_cast<std::int64_t>(w) * rowsPerWriter;
            for (std::int64_t i = 0; i < rowsPerWriter; ++i) {
                insertUser(table, base + i);
            }
        });
    }
    // Readers continuously point-read and scan while writers run.
    for (int r = 0; r < 4; ++r) {
        threads.emplace_back([&] {
            for (int i = 0; i < 500; ++i) {
                auto row = table.findById(i % 100);
                if (row && row->getInt("id") != i % 100) readMismatch.store(true);
                (void)table.selectWhere(
                    [](const Row& candidate) { return candidate.getInt("id") % 7 == 0; });
            }
        });
    }
    for (auto& t : threads) t.join();

    CHECK(!readMismatch.load());
    CHECK_EQ(table.rowCount(), static_cast<std::size_t>(writerThreads * rowsPerWriter));

    // Concurrent deleters splitting the id space remove everything exactly once.
    std::atomic<int> deleted{0};
    std::vector<std::thread> deleters;
    for (int d = 0; d < 4; ++d) {
        deleters.emplace_back([&, d] {
            for (std::int64_t id = d; id < writerThreads * rowsPerWriter; id += 4) {
                if (table.deleteById(id)) deleted.fetch_add(1);
            }
        });
    }
    for (auto& t : deleters) t.join();
    CHECK_EQ(deleted.load(), writerThreads * rowsPerWriter);
    CHECK_EQ(table.rowCount(), 0u);
}

static void testConcurrentDatabase() {
    Database db;
    const int threads = 8;
    std::atomic<int> created{0};

    // Threads race to create/drop/use the same small set of table names.
    std::vector<std::thread> workers;
    for (int t = 0; t < threads; ++t) {
        workers.emplace_back([&, t] {
            for (int i = 0; i < 200; ++i) {
                const std::string name = "t" + std::to_string(i % 5);
                if (db.createTable(name, userSchema())) created.fetch_add(1);
                SharedPtr<Table> table = db.getTable(name);
                if (table) {
                    // The handle stays valid even if another thread drops the
                    // table this instant.
                    (void)table->rowCount();
                }
                if (t == 0 && i % 10 == 9) db.dropTable(name);
            }
        });
    }
    for (auto& w : workers) w.join();

    CHECK(created.load() >= 5);
    CHECK(db.tableCount() <= 5u);
}

int main() {
    testConcurrentTable();
    testConcurrentDatabase();
    return testfw::testSummary("concurrent_table");
}
