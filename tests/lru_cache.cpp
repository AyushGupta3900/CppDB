#include "structures/LRUCache.hpp"

#include <algorithm>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "test_framework.hpp"

using cppdb::LRUCache;

static void testBasics() {
    CHECK_THROWS((LRUCache<int, int>(0)), std::invalid_argument);

    LRUCache<int, std::string> cache(3);
    CHECK_EQ(cache.size(), 0u);
    CHECK_EQ(cache.capacity(), 3u);
    CHECK(cache.get(1) == nullptr);
    CHECK_EQ(cache.misses(), 1u);

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");
    CHECK_EQ(cache.size(), 3u);
    CHECK_EQ(*cache.get(1), "one");
    CHECK_EQ(cache.hits(), 1u);

    // Update in place: value changes, size does not
    cache.put(2, "TWO");
    CHECK_EQ(*cache.get(2), "TWO");
    CHECK_EQ(cache.size(), 3u);
}

static void testEvictionOrder() {
    LRUCache<int, int> cache(3);
    cache.put(1, 10);
    cache.put(2, 20);
    cache.put(3, 30);

    // Touch 1 so 2 becomes the LRU entry
    CHECK(cache.get(1) != nullptr);
    cache.put(4, 40);  // evicts 2
    CHECK(cache.get(2) == nullptr);
    CHECK(cache.get(1) != nullptr);
    CHECK(cache.get(3) != nullptr);
    CHECK(cache.get(4) != nullptr);

    // put() on an existing key also promotes it
    cache.put(1, 11);          // order now: 1, 4, 3
    cache.put(5, 50);          // evicts 3
    CHECK(cache.get(3) == nullptr);
    CHECK_EQ(*cache.get(1), 11);

    // peek does not promote and does not count
    LRUCache<int, int> peeky(2);
    peeky.put(1, 10);
    peeky.put(2, 20);  // order: 2, 1
    CHECK_EQ(*peeky.peek(1), 10);
    const auto missesBefore = peeky.misses();
    peeky.put(3, 30);  // 1 is still LRU -> evicted despite the peek
    CHECK(peeky.peek(1) == nullptr);
    CHECK_EQ(peeky.misses(), missesBefore);
}

static void testEraseAndClear() {
    LRUCache<int, int> cache(2);
    cache.put(1, 10);
    cache.put(2, 20);
    CHECK(cache.erase(1));
    CHECK(!cache.erase(1));
    CHECK(!cache.contains(1));
    CHECK_EQ(cache.size(), 1u);
    cache.put(3, 30);
    cache.put(4, 40);  // evicts 2
    CHECK(!cache.contains(2));
    cache.clear();
    CHECK_EQ(cache.size(), 0u);
    CHECK(cache.get(3) == nullptr);
}

static void testCapacityOne() {
    LRUCache<std::string, int> cache(1);
    cache.put("a", 1);
    cache.put("b", 2);  // evicts a
    CHECK(cache.get("a") == nullptr);
    CHECK_EQ(*cache.get("b"), 2);
    cache.put("b", 3);  // update, not evict
    CHECK_EQ(*cache.get("b"), 3);
}

// Randomized ops checked against a slow-but-obviously-correct model.
static void testAgainstReferenceModel() {
    constexpr std::size_t kCapacity = 8;
    LRUCache<int, int> cache(kCapacity);
    std::vector<std::pair<int, int>> model;  // front = most recent

    auto modelFind = [&](int key) {
        return std::find_if(model.begin(), model.end(),
                            [&](const auto& e) { return e.first == key; });
    };

    std::mt19937 rng(20260611);
    std::uniform_int_distribution<int> keyDist(0, 19);
    std::uniform_int_distribution<int> opDist(0, 2);

    for (int step = 0; step < 20'000; ++step) {
        const int key = keyDist(rng);
        switch (opDist(rng)) {
            case 0: {  // put
                const int value = step;
                cache.put(key, value);
                if (auto it = modelFind(key); it != model.end()) model.erase(it);
                model.insert(model.begin(), {key, value});
                if (model.size() > kCapacity) model.pop_back();
                break;
            }
            case 1: {  // get
                int* got = cache.get(key);
                auto it = modelFind(key);
                if (it == model.end()) {
                    if (got != nullptr) { CHECK(false); return; }
                } else {
                    if (got == nullptr || *got != it->second) { CHECK(false); return; }
                    auto entry = *it;
                    model.erase(it);
                    model.insert(model.begin(), entry);
                }
                break;
            }
            case 2: {  // erase
                const bool cacheHad = cache.erase(key);
                auto it = modelFind(key);
                const bool modelHad = it != model.end();
                if (modelHad) model.erase(it);
                if (cacheHad != modelHad) { CHECK(false); return; }
                break;
            }
        }
        if (cache.size() != model.size()) { CHECK(false); return; }
    }
    CHECK(true);  // model agreed for all 20k steps
}

int main() {
    testBasics();
    testEvictionOrder();
    testEraseAndClear();
    testCapacityOne();
    testAgainstReferenceModel();
    return testfw::testSummary("lru_cache");
}
