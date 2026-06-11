#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <stdexcept>
#include <utility>
#include <vector>

#include "memory/UniquePtr.hpp"

namespace cppdb {

// B-tree keyed map (CLRS-style, minimum degree t).
//
// Every node holds between t-1 and 2t-1 sorted keys (the root may hold
// fewer) and parallel values; internal nodes hold keyCount+1 children. All
// leaves sit at the same depth, so search/insert/remove are O(log n) with
// wide nodes — far fewer pointer hops (and cache misses) than a binary tree,
// and in-order traversal gives sorted output and cheap range scans.
//
// Children are owned through the project's own UniquePtr. Not thread-safe;
// Table wraps it in an RWLock. search() pointers are invalidated by any
// subsequent insert/remove.
template <typename K, typename V>
class BTree {
public:
    explicit BTree(int minDegree = 16) : t_(minDegree) {
        if (minDegree < 2) {
            throw std::invalid_argument("BTree: minimum degree must be >= 2");
        }
    }

    // Returns false (and changes nothing) if the key already exists.
    bool insert(K key, V value) {
        if (!root_) {
            root_ = makeUnique<Node>();
            root_->keys.push_back(std::move(key));
            root_->values.push_back(std::move(value));
            size_ = 1;
            return true;
        }
        if (isFull(*root_)) {
            // Preemptive root split: the tree grows in height here and only here.
            UniquePtr<Node> newRoot = makeUnique<Node>();
            newRoot->children.push_back(std::move(root_));
            root_ = std::move(newRoot);
            splitChild(*root_, 0);
        }
        const bool inserted = insertNonFull(*root_, std::move(key), std::move(value));
        if (inserted) ++size_;
        return inserted;
    }

    V* search(const K& key) {
        Node* node = root_.get();
        while (node != nullptr) {
            const std::size_t i = lowerBound(*node, key);
            if (i < node->keys.size() && node->keys[i] == key) return &node->values[i];
            node = node->isLeaf() ? nullptr : node->children[i].get();
        }
        return nullptr;
    }

    const V* search(const K& key) const {
        return const_cast<BTree*>(this)->search(key);
    }

    bool contains(const K& key) const { return search(key) != nullptr; }

    bool remove(const K& key) {
        if (!root_) return false;
        const bool removed = removeFrom(*root_, key);
        if (removed) --size_;
        if (root_->keys.empty()) {
            // Shrink height: an empty internal root hands over to its only child.
            root_ = root_->isLeaf() ? UniquePtr<Node>{} : std::move(root_->children[0]);
        }
        return removed;
    }

    std::size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }

    // In-order: f(const K&, const V&) in ascending key order.
    template <typename F>
    void forEachInOrder(F&& f) const {
        if (root_) inOrder(*root_, f);
    }

    // Visits keys in [lo, hi] inclusive, pruning subtrees outside the range.
    template <typename F>
    void forEachRange(const K& lo, const K& hi, F&& f) const {
        if (root_) rangeVisit(*root_, lo, hi, f);
    }

    int height() const {
        int h = 0;
        const Node* node = root_.get();
        while (node != nullptr) {
            ++h;
            node = node->isLeaf() ? nullptr : node->children[0].get();
        }
        return h;
    }

    // Debug/test hook: verifies every B-tree invariant (sorted keys, node
    // occupancy bounds, child counts, uniform leaf depth, key separation).
    bool checkInvariants() const {
        if (!root_) return size_ == 0;
        int leafDepth = -1;
        std::size_t counted = 0;
        return checkNode(*root_, /*depth=*/0, /*isRoot=*/true, nullptr, nullptr,
                         leafDepth, counted) &&
               counted == size_;
    }

private:
    struct Node {
        std::vector<K> keys;
        std::vector<V> values;                  // parallel to keys
        std::vector<UniquePtr<Node>> children;  // empty iff leaf

        bool isLeaf() const { return children.empty(); }
    };

    bool isFull(const Node& node) const {
        return node.keys.size() == static_cast<std::size_t>(2 * t_ - 1);
    }

    static std::size_t lowerBound(const Node& node, const K& key) {
        return static_cast<std::size_t>(
            std::lower_bound(node.keys.begin(), node.keys.end(), key) -
            node.keys.begin());
    }

    // Splits the full child parent.children[i]; the median key moves up into
    // parent at position i, the upper half becomes a new right sibling.
    void splitChild(Node& parent, std::size_t i) {
        Node& child = *parent.children[i];
        UniquePtr<Node> right = makeUnique<Node>();
        const std::size_t mid = static_cast<std::size_t>(t_ - 1);

        right->keys.assign(std::make_move_iterator(child.keys.begin() + mid + 1),
                           std::make_move_iterator(child.keys.end()));
        right->values.assign(std::make_move_iterator(child.values.begin() + mid + 1),
                             std::make_move_iterator(child.values.end()));
        if (!child.isLeaf()) {
            right->children.assign(
                std::make_move_iterator(child.children.begin() + mid + 1),
                std::make_move_iterator(child.children.end()));
            child.children.erase(child.children.begin() + mid + 1,
                                 child.children.end());
        }

        K median = std::move(child.keys[mid]);
        V medianValue = std::move(child.values[mid]);
        // erase, not resize: resize would require V to be default-constructible
        child.keys.erase(child.keys.begin() + mid, child.keys.end());
        child.values.erase(child.values.begin() + mid, child.values.end());

        parent.keys.insert(parent.keys.begin() + i, std::move(median));
        parent.values.insert(parent.values.begin() + i, std::move(medianValue));
        parent.children.insert(parent.children.begin() + i + 1, std::move(right));
    }

    bool insertNonFull(Node& node, K key, V value) {
        std::size_t i = lowerBound(node, key);
        if (i < node.keys.size() && node.keys[i] == key) return false;  // duplicate

        if (node.isLeaf()) {
            node.keys.insert(node.keys.begin() + i, std::move(key));
            node.values.insert(node.values.begin() + i, std::move(value));
            return true;
        }
        if (isFull(*node.children[i])) {
            splitChild(node, i);
            if (node.keys[i] == key) return false;  // the median is the key
            if (node.keys[i] < key) ++i;
        }
        return insertNonFull(*node.children[i], std::move(key), std::move(value));
    }

    // ---- removal (CLRS): every recursive call enters a node that is the
    // root or has at least t keys, so deletion never needs to backtrack ----

    bool removeFrom(Node& node, const K& key) {
        std::size_t i = lowerBound(node, key);
        const bool present = i < node.keys.size() && node.keys[i] == key;

        if (present && node.isLeaf()) {
            node.keys.erase(node.keys.begin() + i);
            node.values.erase(node.values.begin() + i);
            return true;
        }
        if (present) {  // internal node
            Node& left = *node.children[i];
            Node& right = *node.children[i + 1];
            if (left.keys.size() >= static_cast<std::size_t>(t_)) {
                // Replace with predecessor, then delete the predecessor below.
                auto [predKey, predValue] = takeMax(left);
                node.keys[i] = std::move(predKey);
                node.values[i] = std::move(predValue);
                return true;
            }
            if (right.keys.size() >= static_cast<std::size_t>(t_)) {
                auto [succKey, succValue] = takeMin(right);
                node.keys[i] = std::move(succKey);
                node.values[i] = std::move(succValue);
                return true;
            }
            mergeChildren(node, i);  // key sinks into the merged child
            return removeFrom(*node.children[i], key);
        }

        if (node.isLeaf()) return false;  // not in the tree

        // Descend; top up the child first if it is at minimum occupancy.
        if (node.children[i]->keys.size() < static_cast<std::size_t>(t_)) {
            fillChild(node, i);
            // A borrow shifts separators and a merge removes one; recomputing
            // the descent index handles every case uniformly.
            i = lowerBound(node, key);
            if (i < node.keys.size() && node.keys[i] == key) {
                return removeFrom(node, key);  // defensive: key surfaced here
            }
        }
        return removeFrom(*node.children[i], key);
    }

    // Removes and returns the maximum entry of the subtree rooted at `node`,
    // maintaining the >= t invariant on the way down.
    std::pair<K, V> takeMax(Node& node) {
        if (node.isLeaf()) {
            std::pair<K, V> result{std::move(node.keys.back()),
                                   std::move(node.values.back())};
            node.keys.pop_back();
            node.values.pop_back();
            return result;
        }
        std::size_t last = node.children.size() - 1;
        if (node.children[last]->keys.size() < static_cast<std::size_t>(t_)) {
            fillChild(node, last);
            last = node.children.size() - 1;  // a merge may have shrunk us
        }
        return takeMax(*node.children[last]);
    }

    std::pair<K, V> takeMin(Node& node) {
        if (node.isLeaf()) {
            std::pair<K, V> result{std::move(node.keys.front()),
                                   std::move(node.values.front())};
            node.keys.erase(node.keys.begin());
            node.values.erase(node.values.begin());
            return result;
        }
        if (node.children[0]->keys.size() < static_cast<std::size_t>(t_)) {
            fillChild(node, 0);
        }
        return takeMin(*node.children[0]);
    }

    // Ensures children[i] has at least t keys: borrow from a sibling through
    // the parent, or merge with one.
    void fillChild(Node& node, std::size_t i) {
        if (i > 0 &&
            node.children[i - 1]->keys.size() >= static_cast<std::size_t>(t_)) {
            borrowFromPrev(node, i);
        } else if (i < node.children.size() - 1 &&
                   node.children[i + 1]->keys.size() >= static_cast<std::size_t>(t_)) {
            borrowFromNext(node, i);
        } else if (i > 0) {
            mergeChildren(node, i - 1);  // merge into the left sibling
        } else {
            mergeChildren(node, i);  // merge the right sibling into us
        }
    }

    // Rotate right: parent separator drops into child, left sibling's last
    // key rises into the parent.
    void borrowFromPrev(Node& node, std::size_t i) {
        Node& child = *node.children[i];
        Node& left = *node.children[i - 1];

        child.keys.insert(child.keys.begin(), std::move(node.keys[i - 1]));
        child.values.insert(child.values.begin(), std::move(node.values[i - 1]));
        node.keys[i - 1] = std::move(left.keys.back());
        node.values[i - 1] = std::move(left.values.back());
        left.keys.pop_back();
        left.values.pop_back();
        if (!left.isLeaf()) {
            child.children.insert(child.children.begin(),
                                  std::move(left.children.back()));
            left.children.pop_back();
        }
    }

    void borrowFromNext(Node& node, std::size_t i) {
        Node& child = *node.children[i];
        Node& right = *node.children[i + 1];

        child.keys.push_back(std::move(node.keys[i]));
        child.values.push_back(std::move(node.values[i]));
        node.keys[i] = std::move(right.keys.front());
        node.values[i] = std::move(right.values.front());
        right.keys.erase(right.keys.begin());
        right.values.erase(right.values.begin());
        if (!right.isLeaf()) {
            child.children.push_back(std::move(right.children.front()));
            right.children.erase(right.children.begin());
        }
    }

    // Merges children[i], the separator key i, and children[i+1] into
    // children[i] (both children have t-1 keys when this is called).
    void mergeChildren(Node& node, std::size_t i) {
        Node& left = *node.children[i];
        Node& right = *node.children[i + 1];

        left.keys.push_back(std::move(node.keys[i]));
        left.values.push_back(std::move(node.values[i]));
        left.keys.insert(left.keys.end(),
                         std::make_move_iterator(right.keys.begin()),
                         std::make_move_iterator(right.keys.end()));
        left.values.insert(left.values.end(),
                           std::make_move_iterator(right.values.begin()),
                           std::make_move_iterator(right.values.end()));
        if (!right.isLeaf()) {
            left.children.insert(left.children.end(),
                                 std::make_move_iterator(right.children.begin()),
                                 std::make_move_iterator(right.children.end()));
        }
        node.keys.erase(node.keys.begin() + i);
        node.values.erase(node.values.begin() + i);
        node.children.erase(node.children.begin() + i + 1);
    }

    template <typename F>
    void inOrder(const Node& node, F& f) const {
        for (std::size_t i = 0; i < node.keys.size(); ++i) {
            if (!node.isLeaf()) inOrder(*node.children[i], f);
            f(node.keys[i], node.values[i]);
        }
        if (!node.isLeaf()) inOrder(*node.children.back(), f);
    }

    template <typename F>
    void rangeVisit(const Node& node, const K& lo, const K& hi, F& f) const {
        for (std::size_t i = lowerBound(node, lo); i <= node.keys.size(); ++i) {
            if (!node.isLeaf()) rangeVisit(*node.children[i], lo, hi, f);
            if (i == node.keys.size() || hi < node.keys[i]) return;
            f(node.keys[i], node.values[i]);
        }
    }

    bool checkNode(const Node& node, int depth, bool isRoot, const K* lowerLimit,
                   const K* upperLimit, int& leafDepth, std::size_t& counted) const {
        const std::size_t n = node.keys.size();
        if (node.values.size() != n) return false;
        if (!isRoot && n < static_cast<std::size_t>(t_ - 1)) return false;
        if (n > static_cast<std::size_t>(2 * t_ - 1)) return false;
        if (isRoot && n == 0) return false;  // an empty root must be null instead

        for (std::size_t i = 0; i < n; ++i) {
            if (i > 0 && !(node.keys[i - 1] < node.keys[i])) return false;
            if (lowerLimit != nullptr && !(*lowerLimit < node.keys[i])) return false;
            if (upperLimit != nullptr && !(node.keys[i] < *upperLimit)) return false;
        }
        counted += n;

        if (node.isLeaf()) {
            if (leafDepth == -1) leafDepth = depth;
            return leafDepth == depth;  // all leaves at one depth
        }
        if (node.children.size() != n + 1) return false;
        for (std::size_t i = 0; i <= n; ++i) {
            const K* childLower = i == 0 ? lowerLimit : &node.keys[i - 1];
            const K* childUpper = i == n ? upperLimit : &node.keys[i];
            if (!node.children[i]) return false;
            if (!checkNode(*node.children[i], depth + 1, false, childLower,
                           childUpper, leafDepth, counted)) {
                return false;
            }
        }
        return true;
    }

    UniquePtr<Node> root_;
    int t_;
    std::size_t size_ = 0;
};

}  // namespace cppdb
