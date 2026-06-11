#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

namespace cppdb {

// Open-addressing hash map with linear probing and tombstone deletion.
// Capacity is always a power of two so the probe index is `hash & mask`.
//
// Invalidation contract (like std::unordered_map but stricter): any insert
// may rehash and invalidate every pointer returned by find()/operator[].
template <typename K, typename V, typename Hash = std::hash<K>>
class HashMap {
public:
    HashMap() : HashMap(kMinCapacity) {}

    explicit HashMap(std::size_t initialCapacity) {
        std::size_t capacity = kMinCapacity;
        while (capacity < initialCapacity) capacity *= 2;
        slots_.resize(capacity);
    }

    // Inserts only if the key is absent. Returns true on insert.
    bool insert(K key, V value) {
        reserveForOneMore();
        return place(std::move(key), std::move(value), /*assignIfPresent=*/false);
    }

    // Inserts or overwrites. Returns true if a new key was inserted.
    bool insertOrAssign(K key, V value) {
        reserveForOneMore();
        return place(std::move(key), std::move(value), /*assignIfPresent=*/true);
    }

    V* find(const K& key) noexcept {
        const std::size_t index = findSlot(key);
        return index == kNotFound ? nullptr : &slots_[index].kv->second;
    }

    const V* find(const K& key) const noexcept {
        const std::size_t index = findSlot(key);
        return index == kNotFound ? nullptr : &slots_[index].kv->second;
    }

    bool contains(const K& key) const noexcept { return findSlot(key) != kNotFound; }

    bool erase(const K& key) {
        const std::size_t index = findSlot(key);
        if (index == kNotFound) return false;
        slots_[index].state = SlotState::DELETED;
        slots_[index].kv.reset();
        --size_;
        return true;
    }

    // Default-constructs the value for a missing key (requires V{}).
    V& operator[](const K& key) {
        if (V* existing = find(key)) return *existing;
        insert(key, V{});
        return *find(key);
    }

    std::size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }
    std::size_t capacity() const noexcept { return slots_.size(); }
    double loadFactor() const noexcept {
        return static_cast<double>(size_) / static_cast<double>(slots_.size());
    }

    // f(const K&, V&) — order is unspecified. Do not insert/erase inside f.
    template <typename F>
    void forEach(F&& f) {
        for (Slot& slot : slots_) {
            if (slot.state == SlotState::OCCUPIED) f(slot.kv->first, slot.kv->second);
        }
    }

    template <typename F>
    void forEach(F&& f) const {
        for (const Slot& slot : slots_) {
            if (slot.state == SlotState::OCCUPIED) f(slot.kv->first, slot.kv->second);
        }
    }

private:
    enum class SlotState : std::uint8_t { EMPTY, OCCUPIED, DELETED };

    struct Slot {
        SlotState state = SlotState::EMPTY;
        std::optional<std::pair<K, V>> kv;
    };

    static constexpr std::size_t kMinCapacity = 16;
    static constexpr std::size_t kNotFound = static_cast<std::size_t>(-1);

    std::size_t mask() const noexcept { return slots_.size() - 1; }

    std::size_t homeIndex(const K& key) const noexcept { return hash_(key) & mask(); }

    // Index of the OCCUPIED slot holding `key`, or kNotFound. Probing stops at
    // the first EMPTY slot; DELETED slots keep the probe chain alive.
    std::size_t findSlot(const K& key) const noexcept {
        std::size_t index = homeIndex(key);
        for (std::size_t probes = 0; probes < slots_.size(); ++probes) {
            const Slot& slot = slots_[index];
            if (slot.state == SlotState::EMPTY) return kNotFound;
            if (slot.state == SlotState::OCCUPIED && slot.kv->first == key) return index;
            index = (index + 1) & mask();
        }
        return kNotFound;
    }

    bool place(K key, V value, bool assignIfPresent) {
        std::size_t index = homeIndex(key);
        std::size_t firstDeleted = kNotFound;
        while (true) {
            Slot& slot = slots_[index];
            if (slot.state == SlotState::OCCUPIED && slot.kv->first == key) {
                if (assignIfPresent) slot.kv->second = std::move(value);
                return false;
            }
            if (slot.state == SlotState::DELETED && firstDeleted == kNotFound) {
                firstDeleted = index;
            }
            if (slot.state == SlotState::EMPTY) {
                // Reuse the earliest tombstone on this probe chain if any.
                const std::size_t target = firstDeleted != kNotFound ? firstDeleted : index;
                Slot& dest = slots_[target];
                if (dest.state == SlotState::EMPTY) ++used_;
                dest.state = SlotState::OCCUPIED;
                dest.kv.emplace(std::move(key), std::move(value));
                ++size_;
                return true;
            }
            index = (index + 1) & mask();
        }
    }

    // Grow (or rebuild to clear tombstones) before used slots pass ~70%.
    void reserveForOneMore() {
        if ((used_ + 1) * 10 <= slots_.size() * 7) return;
        const bool mostlyLive = (size_ + 1) * 10 > slots_.size() * 4;
        rehash(mostlyLive ? slots_.size() * 2 : slots_.size());
    }

    void rehash(std::size_t newCapacity) {
        std::vector<Slot> old = std::move(slots_);
        slots_.assign(newCapacity, Slot{});
        size_ = 0;
        used_ = 0;
        for (Slot& slot : old) {
            if (slot.state == SlotState::OCCUPIED) {
                place(std::move(slot.kv->first), std::move(slot.kv->second),
                      /*assignIfPresent=*/false);
            }
        }
    }

    std::vector<Slot> slots_;
    std::size_t size_ = 0;  // occupied slots
    std::size_t used_ = 0;  // occupied + deleted (tombstones)
    Hash hash_;
};

}  // namespace cppdb
