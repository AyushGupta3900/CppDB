#include "concurrency/LockFreeQueue.hpp"

#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include "memory/UniquePtr.hpp"
#include "test_framework.hpp"

using cppdb::LockFreeQueue;

// FIFO order and empty behavior, single-threaded.
static void testBasics() {
    LockFreeQueue<int> queue;
    int out = 0;
    CHECK(queue.empty());
    CHECK(!queue.pop(out));

    queue.push(1);
    queue.push(2);
    queue.push(3);
    CHECK(!queue.empty());
    CHECK(queue.pop(out));
    CHECK_EQ(out, 1);
    CHECK(queue.pop(out));
    CHECK_EQ(out, 2);
    CHECK(queue.pop(out));
    CHECK_EQ(out, 3);
    CHECK(!queue.pop(out));
    CHECK(queue.empty());

    // Interleaved push/pop
    queue.push(4);
    CHECK(queue.pop(out));
    CHECK_EQ(out, 4);
}

// Move-only payloads work (the queue never copies).
static void testMoveOnlyPayload() {
    LockFreeQueue<cppdb::UniquePtr<int>> queue;
    queue.push(cppdb::makeUnique<int>(7));
    queue.push(cppdb::makeUnique<int>(8));
    cppdb::UniquePtr<int> out;
    CHECK(queue.pop(out));
    CHECK_EQ(*out, 7);
    CHECK(queue.pop(out));
    CHECK_EQ(*out, 8);
}

// Destroying a non-empty queue must not leak (run under ASan/leaks to verify).
static void testDestructionWithItems() {
    LockFreeQueue<std::string> queue;
    for (int i = 0; i < 100; ++i) queue.push("item" + std::to_string(i));
}

// 4 producers, 1 consumer. Every item arrives exactly once, and items from
// the same producer arrive in the order that producer pushed them.
static void testMpscStress() {
    constexpr int kProducers = 4;
    constexpr int kPerProducer = 25'000;
    constexpr std::int64_t kStride = 1'000'000;

    LockFreeQueue<std::int64_t> queue;
    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&queue, p] {
            for (int i = 0; i < kPerProducer; ++i) {
                queue.push(static_cast<std::int64_t>(p) * kStride + i);
            }
        });
    }

    std::vector<std::int64_t> lastSeen(kProducers, -1);
    int received = 0;
    bool orderOk = true;
    while (received < kProducers * kPerProducer) {
        std::int64_t item = 0;
        if (!queue.pop(item)) {
            std::this_thread::yield();
            continue;
        }
        const int producer = static_cast<int>(item / kStride);
        const std::int64_t seq = item % kStride;
        if (seq <= lastSeen[producer]) orderOk = false;  // per-producer FIFO
        lastSeen[producer] = seq;
        ++received;
    }
    for (auto& t : producers) t.join();

    CHECK(orderOk);
    CHECK_EQ(received, kProducers * kPerProducer);
    for (int p = 0; p < kProducers; ++p) {
        CHECK_EQ(lastSeen[p], static_cast<std::int64_t>(kPerProducer - 1));
    }
    CHECK(queue.empty());
}

int main() {
    testBasics();
    testMoveOnlyPayload();
    testDestructionWithItems();
    testMpscStress();
    return testfw::testSummary("lockfree_queue");
}
