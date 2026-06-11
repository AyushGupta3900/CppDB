#include "storage/PoolAllocator.hpp"

#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "test_framework.hpp"

using cppdb::PoolAllocator;

namespace {

struct Tracked {
    static int liveCount;
    static int totalConstructed;
    int value;
    std::string label;

    Tracked(int v, std::string l) : value(v), label(std::move(l)) {
        ++liveCount;
        ++totalConstructed;
    }
    ~Tracked() { --liveCount; }
};

int Tracked::liveCount = 0;
int Tracked::totalConstructed = 0;

struct ThrowingCtor {
    explicit ThrowingCtor(bool doThrow) {
        if (doThrow) throw std::runtime_error("ctor failed");
    }
};

struct alignas(32) OverAligned {
    double values[4];
};

}  // namespace

int main() {
    CHECK_THROWS(PoolAllocator<int>(0), std::invalid_argument);

    // Basic allocate/deallocate with constructor forwarding
    PoolAllocator<Tracked> pool(4);
    CHECK_EQ(pool.capacity(), 4u);
    CHECK_EQ(pool.inUse(), 0u);

    Tracked* a = pool.allocate(1, "a");
    Tracked* b = pool.allocate(2, "b");
    CHECK_EQ(pool.inUse(), 2u);
    CHECK_EQ(pool.available(), 2u);
    CHECK_EQ(a->value, 1);
    CHECK_EQ(b->label, "b");
    CHECK_EQ(Tracked::liveCount, 2);
    CHECK(pool.owns(a));
    CHECK(pool.owns(b));

    // Destructor really runs on deallocate
    pool.deallocate(b);
    CHECK_EQ(Tracked::liveCount, 1);
    CHECK_EQ(pool.inUse(), 1u);

    // LIFO reuse: the freed block is handed out again
    Tracked* c = pool.allocate(3, "c");
    CHECK(c == b);

    // Exhaustion throws bad_alloc and keeps state consistent
    Tracked* d = pool.allocate(4, "d");
    Tracked* e = pool.allocate(5, "e");
    CHECK_EQ(pool.available(), 0u);
    CHECK_THROWS(pool.allocate(6, "f"), std::bad_alloc);
    CHECK_EQ(pool.inUse(), 4u);

    pool.deallocate(a);
    pool.deallocate(c);
    pool.deallocate(d);
    pool.deallocate(e);
    CHECK_EQ(Tracked::liveCount, 0);
    CHECK_EQ(pool.inUse(), 0u);

    // deallocate(nullptr) is a no-op
    pool.deallocate(nullptr);
    CHECK_EQ(pool.inUse(), 0u);

    // A throwing constructor must not leak the block
    PoolAllocator<ThrowingCtor> throwing(1);
    CHECK_THROWS(throwing.allocate(true), std::runtime_error);
    CHECK_EQ(throwing.inUse(), 0u);
    ThrowingCtor* ok = throwing.allocate(false);  // block was recovered
    CHECK(ok != nullptr);
    throwing.deallocate(ok);

    // Over-aligned types: every block must satisfy alignof(T)
    PoolAllocator<OverAligned> aligned(8);
    std::vector<OverAligned*> blocks;
    for (int i = 0; i < 8; ++i) {
        OverAligned* p = aligned.allocate();
        CHECK_EQ(reinterpret_cast<std::uintptr_t>(p) % alignof(OverAligned), 0u);
        blocks.push_back(p);
    }
    for (OverAligned* p : blocks) aligned.deallocate(p);

    // Tiny types: block still fits the intrusive free-list pointer
    PoolAllocator<char> tiny(16);
    char* c1 = tiny.allocate('x');
    char* c2 = tiny.allocate('y');
    CHECK_EQ(*c1, 'x');
    CHECK_EQ(*c2, 'y');
    tiny.deallocate(c1);
    tiny.deallocate(c2);

    // owns() rejects foreign pointers
    int local = 0;
    PoolAllocator<int> ints(2);
    CHECK(!ints.owns(&local));

    // Move semantics transfer the arena
    PoolAllocator<int> source(2);
    int* kept = source.allocate(7);
    PoolAllocator<int> moved(std::move(source));
    CHECK(moved.owns(kept));
    CHECK_EQ(moved.inUse(), 1u);
    CHECK_EQ(*kept, 7);
    moved.deallocate(kept);

    // Churn: allocate/free cycles never exhaust a balanced workload
    PoolAllocator<Tracked> churn(8);
    for (int round = 0; round < 1000; ++round) {
        Tracked* items[8];
        for (int i = 0; i < 8; ++i) items[i] = churn.allocate(i, "x");
        for (int i = 7; i >= 0; --i) churn.deallocate(items[i]);
    }
    CHECK_EQ(churn.inUse(), 0u);
    CHECK_EQ(Tracked::liveCount, 0);

    return testfw::testSummary("pool_allocator");
}
