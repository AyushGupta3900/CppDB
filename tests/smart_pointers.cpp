#include <atomic>
#include <thread>
#include <utility>
#include <vector>

#include "memory/SharedPtr.hpp"
#include "memory/UniquePtr.hpp"
#include "test_framework.hpp"

using cppdb::makeShared;
using cppdb::makeUnique;
using cppdb::SharedPtr;
using cppdb::UniquePtr;

namespace {

struct Tracked {
    static std::atomic<int> liveCount;
    int value;

    explicit Tracked(int v) : value(v) { liveCount.fetch_add(1); }
    ~Tracked() { liveCount.fetch_sub(1); }
};

std::atomic<int> Tracked::liveCount{0};

}  // namespace

static void testUniquePtr() {
    // Lifecycle: destructor runs exactly once, at scope exit
    {
        UniquePtr<Tracked> p = makeUnique<Tracked>(7);
        CHECK(static_cast<bool>(p));
        CHECK_EQ(p->value, 7);
        CHECK_EQ((*p).value, 7);
        CHECK_EQ(Tracked::liveCount.load(), 1);
    }
    CHECK_EQ(Tracked::liveCount.load(), 0);

    // Default is empty
    UniquePtr<Tracked> empty;
    CHECK(!empty);
    CHECK(empty == nullptr);
    CHECK(empty.get() == nullptr);

    // Move constructor transfers ownership
    UniquePtr<Tracked> a = makeUnique<Tracked>(1);
    Tracked* raw = a.get();
    UniquePtr<Tracked> b(std::move(a));
    CHECK(a.get() == nullptr);
    CHECK(b.get() == raw);
    CHECK_EQ(Tracked::liveCount.load(), 1);

    // Move assignment deletes the target's old object
    UniquePtr<Tracked> c = makeUnique<Tracked>(2);
    CHECK_EQ(Tracked::liveCount.load(), 2);
    c = std::move(b);
    CHECK_EQ(Tracked::liveCount.load(), 1);  // the value-2 object died
    CHECK_EQ(c->value, 1);
    CHECK(b.get() == nullptr);

    // release() hands back the raw pointer without deleting
    Tracked* released = c.release();
    CHECK(c.get() == nullptr);
    CHECK_EQ(Tracked::liveCount.load(), 1);
    delete released;
    CHECK_EQ(Tracked::liveCount.load(), 0);

    // reset() swaps in a new object, deleting the old
    UniquePtr<Tracked> d = makeUnique<Tracked>(3);
    d.reset(new Tracked(4));
    CHECK_EQ(Tracked::liveCount.load(), 1);
    CHECK_EQ(d->value, 4);
    d.reset();
    CHECK(!d);
    CHECK_EQ(Tracked::liveCount.load(), 0);

    // swap
    UniquePtr<Tracked> x = makeUnique<Tracked>(10);
    UniquePtr<Tracked> y = makeUnique<Tracked>(20);
    x.swap(y);
    CHECK_EQ(x->value, 20);
    CHECK_EQ(y->value, 10);

    // Works inside std::vector (move-only element type)
    std::vector<UniquePtr<Tracked>> owners;
    for (int i = 0; i < 100; ++i) owners.push_back(makeUnique<Tracked>(i));
    CHECK_EQ(Tracked::liveCount.load(), 100 + 2);  // +2 for x and y
    CHECK_EQ(owners[42]->value, 42);
    owners.clear();
    x.reset();
    y.reset();
    CHECK_EQ(Tracked::liveCount.load(), 0);
}

static void testSharedPtr() {
    // Count rises with copies and falls at scope exits
    SharedPtr<Tracked> p = makeShared<Tracked>(5);
    CHECK_EQ(p.useCount(), 1L);
    {
        SharedPtr<Tracked> q = p;
        CHECK_EQ(p.useCount(), 2L);
        CHECK_EQ(q->value, 5);
        CHECK(q.get() == p.get());
        SharedPtr<Tracked> r;
        r = q;
        CHECK_EQ(p.useCount(), 3L);
    }
    CHECK_EQ(p.useCount(), 1L);
    CHECK_EQ(Tracked::liveCount.load(), 1);

    // Move does not change the count, source becomes empty
    SharedPtr<Tracked> moved(std::move(p));
    CHECK_EQ(moved.useCount(), 1L);
    CHECK(p.get() == nullptr);
    CHECK_EQ(p.useCount(), 0L);

    // Self-assignment is harmless
    SharedPtr<Tracked>& self = moved;
    moved = self;
    CHECK_EQ(moved.useCount(), 1L);
    CHECK_EQ(Tracked::liveCount.load(), 1);

    // reset(ptr) releases the old object
    moved.reset(new Tracked(6));
    CHECK_EQ(Tracked::liveCount.load(), 1);
    CHECK_EQ(moved->value, 6);
    moved.reset();
    CHECK(!moved);
    CHECK_EQ(Tracked::liveCount.load(), 0);

    // Empty pointers
    SharedPtr<Tracked> empty;
    CHECK_EQ(empty.useCount(), 0L);
    CHECK(!empty);

    // Assigning over the last owner deletes the object
    SharedPtr<Tracked> last = makeShared<Tracked>(8);
    last = SharedPtr<Tracked>();
    CHECK_EQ(Tracked::liveCount.load(), 0);
}

static void testSharedPtrThreaded() {
    // 8 threads copy and drop the same SharedPtr 50k times each. If the
    // refcount were not atomic, the count would corrupt and the object would
    // be deleted twice or leaked.
    SharedPtr<Tracked> shared = makeShared<Tracked>(99);
    const int threads = 8;
    const int iterations = 50'000;
    std::vector<std::thread> workers;
    workers.reserve(threads);
    for (int t = 0; t < threads; ++t) {
        workers.emplace_back([&shared] {
            for (int i = 0; i < iterations; ++i) {
                SharedPtr<Tracked> local = shared;  // atomic increment
                CHECK_EQ(local->value, 99);
            }                                       // atomic decrement
        });
    }
    for (auto& w : workers) w.join();
    CHECK_EQ(shared.useCount(), 1L);
    CHECK_EQ(Tracked::liveCount.load(), 1);
    shared.reset();
    CHECK_EQ(Tracked::liveCount.load(), 0);
}

int main() {
    testUniquePtr();
    testSharedPtr();
    testSharedPtrThreaded();
    return testfw::testSummary("smart_pointers");
}
