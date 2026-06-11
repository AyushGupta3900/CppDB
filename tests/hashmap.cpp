#include "structures/HashMap.hpp"

#include <string>

#include "test_framework.hpp"

using cppdb::HashMap;

namespace {

// Forces every key into the same home slot to exercise probing + tombstones.
struct CollidingHash {
    std::size_t operator()(int) const noexcept { return 42; }
};

}  // namespace

int main() {
    // Basic insert / find / contains
    HashMap<std::string, int> ages;
    CHECK(ages.empty());
    CHECK(ages.insert("alice", 30));
    CHECK(ages.insert("bob", 25));
    CHECK(!ages.insert("alice", 99));  // duplicate insert refused
    CHECK_EQ(ages.size(), 2u);
    CHECK(ages.contains("alice"));
    CHECK(!ages.contains("carol"));
    CHECK(ages.find("missing") == nullptr);

    const int* aliceAge = ages.find("alice");
    CHECK(aliceAge != nullptr);
    CHECK_EQ(*aliceAge, 30);  // value not clobbered by refused insert

    // insertOrAssign overwrites
    CHECK(!ages.insertOrAssign("alice", 31));  // existing -> assigned
    CHECK(ages.insertOrAssign("carol", 28));   // new -> inserted
    CHECK_EQ(*ages.find("alice"), 31);
    CHECK_EQ(ages.size(), 3u);

    // operator[] default-constructs and is writable
    ages["dave"] += 40;
    CHECK_EQ(*ages.find("dave"), 40);
    ages["dave"] = 41;
    CHECK_EQ(ages["dave"], 41);
    CHECK_EQ(ages.size(), 4u);

    // erase
    CHECK(ages.erase("bob"));
    CHECK(!ages.erase("bob"));
    CHECK(!ages.contains("bob"));
    CHECK_EQ(ages.size(), 3u);

    // const access
    const auto& constAges = ages;
    CHECK(constAges.find("alice") != nullptr);
    CHECK(constAges.contains("carol"));

    // forEach visits every live entry exactly once
    int visited = 0;
    int total = 0;
    constAges.forEach([&](const std::string&, const int& v) {
        ++visited;
        total += v;
    });
    CHECK_EQ(visited, 3);
    CHECK_EQ(total, 31 + 28 + 41);

    // Growth: many inserts trigger several rehashes, nothing is lost
    HashMap<int, int> big;
    const int n = 10'000;
    for (int i = 0; i < n; ++i) CHECK(big.insert(i, i * 2));
    CHECK_EQ(big.size(), static_cast<std::size_t>(n));
    bool allPresent = true;
    for (int i = 0; i < n; ++i) {
        const int* v = big.find(i);
        if (v == nullptr || *v != i * 2) allPresent = false;
    }
    CHECK(allPresent);
    CHECK(big.loadFactor() <= 0.7);

    // Delete half, verify the rest survives lookup through tombstones
    for (int i = 0; i < n; i += 2) CHECK(big.erase(i));
    CHECK_EQ(big.size(), static_cast<std::size_t>(n / 2));
    bool oddsAlive = true;
    for (int i = 1; i < n; i += 2) {
        const int* v = big.find(i);
        if (v == nullptr || *v != i * 2) oddsAlive = false;
    }
    CHECK(oddsAlive);
    for (int i = 0; i < n; i += 2) {
        if (big.contains(i)) oddsAlive = false;
    }
    CHECK(oddsAlive);

    // Reinsert into tombstoned slots
    for (int i = 0; i < n; i += 2) CHECK(big.insert(i, -i));
    CHECK_EQ(big.size(), static_cast<std::size_t>(n));
    CHECK_EQ(*big.find(100), -100);
    CHECK_EQ(*big.find(101), 202);

    // Adversarial hash: all keys collide onto one chain
    HashMap<int, std::string, CollidingHash> chain;
    for (int i = 0; i < 50; ++i) chain.insert(i, "v" + std::to_string(i));
    CHECK_EQ(chain.size(), 50u);
    CHECK_EQ(*chain.find(0), "v0");
    CHECK_EQ(*chain.find(49), "v49");
    // Erase from the middle of the chain; later keys must stay reachable
    CHECK(chain.erase(10));
    CHECK(chain.erase(11));
    CHECK_EQ(*chain.find(49), "v49");
    CHECK_EQ(*chain.find(12), "v12");
    // Tombstone reuse: reinsert lands without losing anything
    CHECK(chain.insert(10, "again"));
    CHECK_EQ(*chain.find(10), "again");
    CHECK_EQ(*chain.find(49), "v49");

    // Churn: repeated erase+insert cycles must not degrade or lose data
    HashMap<int, int> churn;
    for (int round = 0; round < 100; ++round) {
        for (int i = 0; i < 64; ++i) churn.insertOrAssign(i, round);
        for (int i = 0; i < 64; i += 2) churn.erase(i);
    }
    CHECK_EQ(churn.size(), 32u);
    CHECK_EQ(*churn.find(1), 99);

    return testfw::testSummary("hashmap");
}
