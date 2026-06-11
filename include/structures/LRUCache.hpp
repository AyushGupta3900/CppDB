#pragma once

#include <cstddef>
#include <list>
#include <stdexcept>
#include <utility>

#include "structures/HashMap.hpp"

namespace cppdb {

// Fixed-capacity least-recently-used cache. Two structures cooperate:
//   - std::list<Entry> ordered by recency (front = hottest, back = next out)
//   - HashMap<K, list::iterator> for O(1) key -> node jumps
// The pairing works because std::list iterators stay valid under splice() —
// moving a node to the front never invalidates the iterator stored in the
// map. A vector or deque could not be used here (their iterators invalidate
// on any insertion/removal — see concepts on iterator invalidation).
//
// Not thread-safe; callers synchronize (Table guards it with a mutex).
template <typename K, typename V>
class LRUCache {
public:
    explicit LRUCache(std::size_t capacity) : capacity_(capacity) {
        if (capacity == 0) {
            throw std::invalid_argument("LRUCache: capacity must be > 0");
        }
    }

    // Inserts or overwrites, marking the key most-recently-used. Evicts the
    // least-recently-used entry if the cache is full.
    void put(const K& key, V value) {
        if (auto* it = index_.find(key)) {
            (*it)->second = std::move(value);
            touch(*it);
            return;
        }
        if (items_.size() == capacity_) {
            index_.erase(items_.back().first);
            items_.pop_back();
        }
        items_.emplace_front(key, std::move(value));
        index_.insertOrAssign(key, items_.begin());
    }

    // nullptr on miss. A hit marks the key most-recently-used. The pointer is
    // valid only until the next put/get/erase.
    V* get(const K& key) {
        auto* it = index_.find(key);
        if (it == nullptr) {
            ++misses_;
            return nullptr;
        }
        ++hits_;
        touch(*it);
        return &items_.front().second;
    }

    // Read without promoting (does not disturb recency or the counters).
    const V* peek(const K& key) const {
        const auto* it = index_.find(key);
        return it == nullptr ? nullptr : &(*it)->second;
    }

    bool contains(const K& key) const { return index_.contains(key); }

    bool erase(const K& key) {
        auto* it = index_.find(key);
        if (it == nullptr) return false;
        items_.erase(*it);
        index_.erase(key);
        return true;
    }

    void clear() {
        items_.clear();
        index_ = {};
    }

    std::size_t size() const noexcept { return items_.size(); }
    std::size_t capacity() const noexcept { return capacity_; }
    std::size_t hits() const noexcept { return hits_; }
    std::size_t misses() const noexcept { return misses_; }

private:
    using Entry = std::pair<K, V>;
    using ListIterator = typename std::list<Entry>::iterator;

    // Move a node to the front in O(1); the iterator stays valid.
    void touch(ListIterator it) { items_.splice(items_.begin(), items_, it); }

    std::size_t capacity_;
    std::list<Entry> items_;
    HashMap<K, ListIterator> index_;
    std::size_t hits_ = 0;
    std::size_t misses_ = 0;
};

}  // namespace cppdb
