#include "structures/BTree.hpp"

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "test_framework.hpp"

using cppdb::BTree;

static void testBasics() {
    CHECK_THROWS((BTree<int, int>(1)), std::invalid_argument);

    BTree<int, std::string> tree(2);  // t=2: splits/merges happen constantly
    CHECK(tree.empty());
    CHECK(!tree.contains(1));
    CHECK(!tree.remove(1));

    CHECK(tree.insert(5, "five"));
    CHECK(tree.insert(1, "one"));
    CHECK(tree.insert(9, "nine"));
    CHECK(tree.insert(3, "three"));
    CHECK(tree.insert(7, "seven"));
    CHECK(!tree.insert(5, "FIVE"));  // duplicate refused
    CHECK_EQ(tree.size(), 5u);
    CHECK_EQ(*tree.search(5), "five");  // original value kept
    CHECK_EQ(*tree.search(1), "one");
    CHECK(tree.search(2) == nullptr);
    CHECK(tree.checkInvariants());

    // In-order traversal is sorted
    std::vector<int> keys;
    tree.forEachInOrder([&](const int& k, const std::string&) { keys.push_back(k); });
    CHECK(std::is_sorted(keys.begin(), keys.end()));
    CHECK_EQ(keys.size(), 5u);

    CHECK(tree.remove(5));
    CHECK(!tree.remove(5));
    CHECK(!tree.contains(5));
    CHECK_EQ(tree.size(), 4u);
    CHECK(tree.checkInvariants());
}

static void testSequentialAndBalance() {
    BTree<int, int> tree(2);
    const int n = 1'000;
    for (int i = 0; i < n; ++i) CHECK(tree.insert(i, i * 10));
    CHECK_EQ(tree.size(), static_cast<std::size_t>(n));
    CHECK(tree.checkInvariants());
    for (int i = 0; i < n; ++i) {
        const int* v = tree.search(i);
        if (v == nullptr || *v != i * 10) {
            CHECK(false);
            break;
        }
    }

    // Balance: with t=16, 100k sequential keys must stay shallow (a BST
    // would degenerate to a 100k-deep list on sorted input).
    BTree<int, int> wide(16);
    for (int i = 0; i < 100'000; ++i) wide.insert(i, i);
    CHECK(wide.checkInvariants());
    CHECK(wide.height() <= 4);
}

static void testRandomChurn() {
    std::mt19937 rng(20260611);
    BTree<std::int64_t, std::int64_t> tree(2);
    std::set<std::int64_t> model;  // reference implementation

    std::vector<std::int64_t> universe(2'000);
    std::iota(universe.begin(), universe.end(), 0);
    std::shuffle(universe.begin(), universe.end(), rng);

    // Insert everything in random order
    for (std::int64_t k : universe) {
        CHECK(tree.insert(k, k * 2));
        model.insert(k);
    }
    CHECK(tree.checkInvariants());
    CHECK_EQ(tree.size(), model.size());

    // Randomly remove half, checking invariants as we go
    std::shuffle(universe.begin(), universe.end(), rng);
    for (std::size_t i = 0; i < universe.size() / 2; ++i) {
        CHECK(tree.remove(universe[i]));
        model.erase(universe[i]);
        if (i % 100 == 0) CHECK(tree.checkInvariants());
    }
    CHECK(tree.checkInvariants());
    CHECK_EQ(tree.size(), model.size());

    // Tree contents must exactly match the reference set, in order
    std::vector<std::int64_t> treeKeys;
    tree.forEachInOrder([&](const std::int64_t& k, const std::int64_t& v) {
        treeKeys.push_back(k);
        if (v != k * 2) treeKeys.push_back(-999);  // corrupt marker
    });
    std::vector<std::int64_t> modelKeys(model.begin(), model.end());
    CHECK(treeKeys == modelKeys);

    // Mixed churn rounds: delete a random present key, insert a fresh one
    std::int64_t next = 10'000;
    std::vector<std::int64_t> present(model.begin(), model.end());
    for (int round = 0; round < 1'000; ++round) {
        std::uniform_int_distribution<std::size_t> pick(0, present.size() - 1);
        const std::size_t victim = pick(rng);
        CHECK(tree.remove(present[victim]));
        present[victim] = next;
        CHECK(tree.insert(next, next * 2));
        ++next;
    }
    CHECK(tree.checkInvariants());
    CHECK_EQ(tree.size(), present.size());

    // Drain to empty through every merge path
    for (std::int64_t k : present) CHECK(tree.remove(k));
    CHECK(tree.empty());
    CHECK(tree.checkInvariants());
}

static void testRangeScan() {
    BTree<int, int> tree(3);
    for (int i = 0; i < 200; i += 2) tree.insert(i, i);  // evens 0..198

    std::vector<int> got;
    tree.forEachRange(50, 60, [&](const int& k, const int&) { got.push_back(k); });
    CHECK(got == (std::vector<int>{50, 52, 54, 56, 58, 60}));

    got.clear();
    tree.forEachRange(51, 53, [&](const int& k, const int&) { got.push_back(k); });
    CHECK(got == (std::vector<int>{52}));  // bounds need not exist

    got.clear();
    tree.forEachRange(-100, 2, [&](const int& k, const int&) { got.push_back(k); });
    CHECK(got == (std::vector<int>{0, 2}));

    got.clear();
    tree.forEachRange(198, 500, [&](const int& k, const int&) { got.push_back(k); });
    CHECK(got == (std::vector<int>{198}));

    got.clear();
    tree.forEachRange(300, 400, [&](const int& k, const int&) { got.push_back(k); });
    CHECK(got.empty());

    // Full-range scan equals in-order traversal
    got.clear();
    tree.forEachRange(0, 198, [&](const int& k, const int&) { got.push_back(k); });
    CHECK_EQ(got.size(), 100u);
    CHECK(std::is_sorted(got.begin(), got.end()));
}

static void testStringKeys() {
    BTree<std::string, int> tree(2);
    tree.insert("banana", 2);
    tree.insert("apple", 1);
    tree.insert("cherry", 3);
    CHECK_EQ(*tree.search("apple"), 1);
    std::vector<std::string> keys;
    tree.forEachInOrder([&](const std::string& k, const int&) { keys.push_back(k); });
    CHECK(keys == (std::vector<std::string>{"apple", "banana", "cherry"}));
    CHECK(tree.remove("banana"));
    CHECK(!tree.contains("banana"));
    CHECK(tree.checkInvariants());
}

int main() {
    testBasics();
    testSequentialAndBalance();
    testRandomChurn();
    testRangeScan();
    testStringKeys();
    return testfw::testSummary("btree");
}
