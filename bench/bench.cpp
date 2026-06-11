// Micro-benchmarks for the numbers in docs/RESUME.md. Build with -O2 via
// `make bench` — benchmarking a -O0 build measures nothing real.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <latch>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "concurrency/LockFreeQueue.hpp"
#include "concurrency/ThreadPool.hpp"
#include "core/Database.hpp"
#include "core/Table.hpp"
#include "network/TCPServer.hpp"
#include "storage/PoolAllocator.hpp"
#include "structures/BTree.hpp"
#include "structures/HashMap.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

using Clock = std::chrono::steady_clock;

namespace {

double secondsSince(Clock::time_point start) {
    return std::chrono::duration<double>(Clock::now() - start).count();
}

struct RowBlock {  // stand-in for a fixed-size record payload
    std::int64_t id;
    char payload[56];
};

cppdb::Schema userSchema() {
    cppdb::Schema schema;
    schema.addColumn("id", cppdb::DataType::INT);
    schema.addColumn("name", cppdb::DataType::TEXT);
    return schema;
}

void benchAllocator() {
    constexpr int kOps = 1'000'000;
    constexpr int kBatch = 1'000;  // allocate a batch, free it, repeat
    std::vector<RowBlock*> ptrs(kBatch);

    auto t0 = Clock::now();
    for (int round = 0; round < kOps / kBatch; ++round) {
        for (int i = 0; i < kBatch; ++i) ptrs[i] = new RowBlock{};
        for (int i = 0; i < kBatch; ++i) delete ptrs[i];
    }
    const double newDelete = secondsSince(t0);

    cppdb::PoolAllocator<RowBlock> pool(kBatch);
    t0 = Clock::now();
    for (int round = 0; round < kOps / kBatch; ++round) {
        for (int i = 0; i < kBatch; ++i) ptrs[i] = pool.allocate();
        for (int i = 0; i < kBatch; ++i) pool.deallocate(ptrs[i]);
    }
    const double pooled = secondsSince(t0);

    std::cout << "PoolAllocator  : " << kOps << " alloc+free of 64B blocks\n"
              << "  new/delete   : " << std::fixed << std::setprecision(1)
              << newDelete * 1e9 / kOps << " ns/op\n"
              << "  pool         : " << pooled * 1e9 / kOps << " ns/op  ("
              << std::setprecision(2) << newDelete / pooled << "x faster)\n\n";
}

void benchLookup() {
    constexpr int kRows = 100'000;
    constexpr int kLookups = 2'000;
    std::mt19937 rng(42);

    std::vector<std::pair<std::int64_t, std::int64_t>> flat;
    cppdb::BTree<std::int64_t, std::int64_t> tree(16);
    for (int i = 0; i < kRows; ++i) {
        flat.push_back({i, i});
        tree.insert(i, i);
    }
    std::uniform_int_distribution<std::int64_t> pick(0, kRows - 1);
    std::vector<std::int64_t> targets(kLookups);
    for (auto& t : targets) t = pick(rng);

    volatile std::int64_t sink = 0;
    auto t0 = Clock::now();
    for (std::int64_t key : targets) {  // O(n) linear scan baseline
        for (const auto& [k, v] : flat) {
            if (k == key) {
                sink += v;
                break;
            }
        }
    }
    const double scan = secondsSince(t0);

    t0 = Clock::now();
    for (std::int64_t key : targets) sink += *tree.search(key);
    const double indexed = secondsSince(t0);

    std::cout << "B-tree index   : " << kLookups << " point lookups in " << kRows
              << " rows\n"
              << "  linear scan  : " << std::setprecision(1)
              << scan * 1e6 / kLookups << " us/lookup\n"
              << "  B-tree       : " << indexed * 1e9 / kLookups << " ns/lookup  ("
              << std::setprecision(0) << scan / indexed << "x faster)\n\n";
}

void benchLruCache() {
    constexpr int kRows = 100'000;
    constexpr int kReads = 200'000;
    constexpr int kHotKeys = 500;

    cppdb::Table table{userSchema(), /*cacheCapacity=*/1'000};
    for (int i = 0; i < kRows; ++i) {
        cppdb::Row row = table.makeRow();
        row.set("id", std::to_string(i));
        row.set("name", "user" + std::to_string(i));
        table.insert(std::move(row));
    }

    // Skewed workload: 90% of reads hit kHotKeys hot ids.
    std::mt19937 rng(7);
    std::uniform_int_distribution<int> coin(0, 9);
    std::uniform_int_distribution<int> hot(0, kHotKeys - 1);
    std::uniform_int_distribution<int> cold(0, kRows - 1);
    std::vector<std::int64_t> reads(kReads);
    for (auto& r : reads) r = coin(rng) < 9 ? hot(rng) : cold(rng);

    auto t0 = Clock::now();
    for (std::int64_t id : reads) (void)table.findById(id);
    const double elapsed = secondsSince(t0);

    const auto hits = table.cacheHits();
    const auto misses = table.cacheMisses();
    std::cout << "LRU row cache  : " << kReads << " skewed reads (90% on "
              << kHotKeys << " hot ids), capacity 1000\n"
              << "  hit rate     : " << std::setprecision(1)
              << 100.0 * static_cast<double>(hits) /
                     static_cast<double>(hits + misses)
              << "%\n"
              << "  throughput   : " << std::setprecision(2)
              << kReads / elapsed / 1e6 << "M reads/sec\n\n";
}

void benchThreadPool() {
    constexpr int kTasks = 200'000;
    std::cout << "ThreadPool     : " << kTasks << " no-op tasks\n";
    for (std::size_t workers : {1u, 2u, 4u, 8u}) {
        std::atomic<int> counter{0};
        std::latch done(kTasks);
        auto t0 = Clock::now();
        {
            cppdb::ThreadPool pool(workers);
            for (int i = 0; i < kTasks; ++i) {
                pool.submit([&] {
                    counter.fetch_add(1, std::memory_order_relaxed);
                    done.count_down();
                });
            }
            done.wait();
        }
        const double elapsed = secondsSince(t0);
        std::cout << "  " << workers << " worker(s)  : " << std::setprecision(2)
                  << kTasks / elapsed / 1e6 << "M tasks/sec\n";
    }
    std::cout << "\n";
}

void benchLockFreeQueue() {
    constexpr int kProducers = 4;
    constexpr int kPerProducer = 250'000;
    cppdb::LockFreeQueue<std::int64_t> queue;

    auto t0 = Clock::now();
    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&queue] {
            for (int i = 0; i < kPerProducer; ++i) queue.push(i);
        });
    }
    std::int64_t received = 0;
    std::int64_t item = 0;
    while (received < static_cast<std::int64_t>(kProducers) * kPerProducer) {
        if (queue.pop(item)) {
            ++received;
        } else {
            std::this_thread::yield();
        }
    }
    for (auto& t : producers) t.join();
    const double elapsed = secondsSince(t0);
    std::cout << "LockFreeQueue  : " << kProducers << " producers, 1 consumer\n"
              << "  throughput   : " << std::setprecision(2)
              << received / elapsed / 1e6 << "M items/sec\n\n";
}

void benchConcurrentClients() {
    constexpr int kClients = 50;
    constexpr int kQueriesEach = 20;

    cppdb::Database db;
    cppdb::TCPServer server(0, db, 4);
    std::thread serverThread([&server] { server.run(); });

    {
        // setup connection
        cppdb::QueryExecutor exec(db);
        exec.execute("CREATE TABLE users (id INT, name TEXT)");
    }

    std::atomic<int> okCount{0};
    auto t0 = Clock::now();
    std::vector<std::thread> clients;
    for (int c = 0; c < kClients; ++c) {
        clients.emplace_back([&, c] {
            const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            addr.sin_port = htons(server.port());
            if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
                ::close(fd);
                return;
            }
            char ch = 0;
            for (int i = 0; i < kQueriesEach; ++i) {
                const int id = c * kQueriesEach + i;
                std::string q = "INSERT INTO users (id, name) VALUES (" +
                                std::to_string(id) + ", 'u')\n";
                (void)::send(fd, q.data(), q.size(), 0);
                std::string line;
                while (::read(fd, &ch, 1) == 1 && ch != '\n') line += ch;
                if (line == "OK") okCount.fetch_add(1);
            }
            ::close(fd);
        });
    }
    for (auto& t : clients) t.join();
    const double elapsed = secondsSince(t0);
    server.stop();
    serverThread.join();

    std::cout << "TCP server     : " << kClients << " concurrent clients, "
              << kQueriesEach << " queries each\n"
              << "  completed    : " << okCount.load() << "/"
              << kClients * kQueriesEach << " inserts\n"
              << "  throughput   : " << std::setprecision(0)
              << okCount.load() / elapsed << " queries/sec end-to-end\n\n";
}

}  // namespace

int main() {
    std::cout << "CppDB benchmarks (arm64, -O2)\n"
              << "--------------------------------\n\n";
    benchAllocator();
    benchLookup();
    benchLruCache();
    benchThreadPool();
    benchLockFreeQueue();
    benchConcurrentClients();
    return 0;
}
