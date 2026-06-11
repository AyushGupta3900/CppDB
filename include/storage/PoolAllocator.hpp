#pragma once

#include <cassert>
#include <cstddef>
#include <new>
#include <stdexcept>
#include <utility>

namespace cppdb {

// Fixed-capacity object pool. One raw arena is carved into equal blocks; free
// blocks form an intrusive singly-linked list (the pointer to the next free
// block lives *inside* the free block itself, so bookkeeping costs zero extra
// memory). allocate() placement-news a T into a block, deallocate() destroys
// it and pushes the block back — both O(1), no malloc/free per object.
//
// Not thread-safe. Destroying the pool while objects are live is a logic
// error (asserted in debug builds).
template <typename T>
class PoolAllocator {
public:
    explicit PoolAllocator(std::size_t capacity) : capacity_(capacity) {
        if (capacity == 0) {
            throw std::invalid_argument("PoolAllocator: capacity must be > 0");
        }
        arena_ = static_cast<std::byte*>(
            ::operator new(capacity_ * kBlockSize, std::align_val_t{kBlockAlign}));
        buildFreeList();
    }

    ~PoolAllocator() { releaseArena(); }

    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    PoolAllocator(PoolAllocator&& other) noexcept
        : arena_(std::exchange(other.arena_, nullptr)),
          freeHead_(std::exchange(other.freeHead_, nullptr)),
          capacity_(std::exchange(other.capacity_, 0)),
          inUse_(std::exchange(other.inUse_, 0)) {}

    PoolAllocator& operator=(PoolAllocator&& other) noexcept {
        if (this != &other) {
            releaseArena();
            arena_ = std::exchange(other.arena_, nullptr);
            freeHead_ = std::exchange(other.freeHead_, nullptr);
            capacity_ = std::exchange(other.capacity_, 0);
            inUse_ = std::exchange(other.inUse_, 0);
        }
        return *this;
    }

    // Constructs a T in a free block. Throws std::bad_alloc when exhausted.
    template <typename... Args>
    T* allocate(Args&&... args) {
        if (freeHead_ == nullptr) throw std::bad_alloc{};
        FreeNode* node = freeHead_;
        FreeNode* next = node->next;  // read before construction overwrites the block
        T* object = nullptr;
        try {
            object = ::new (static_cast<void*>(node)) T(std::forward<Args>(args)...);
        } catch (...) {
            ::new (static_cast<void*>(node)) FreeNode{next};  // restore the free node
            throw;
        }
        freeHead_ = next;
        ++inUse_;
        return object;
    }

    // Destroys the object and returns its block to the free list (LIFO).
    void deallocate(T* ptr) {
        if (ptr == nullptr) return;
        assert(owns(ptr) && "PoolAllocator: pointer is not from this pool");
        ptr->~T();
        freeHead_ = ::new (static_cast<void*>(ptr)) FreeNode{freeHead_};
        --inUse_;
    }

    bool owns(const T* ptr) const noexcept {
        const auto* p = reinterpret_cast<const std::byte*>(ptr);
        return p >= arena_ && p < arena_ + capacity_ * kBlockSize &&
               static_cast<std::size_t>(p - arena_) % kBlockSize == 0;
    }

    std::size_t capacity() const noexcept { return capacity_; }
    std::size_t inUse() const noexcept { return inUse_; }
    std::size_t available() const noexcept { return capacity_ - inUse_; }

private:
    struct FreeNode {
        FreeNode* next;
    };

    static constexpr std::size_t maxOf(std::size_t a, std::size_t b) {
        return a > b ? a : b;
    }
    static constexpr std::size_t roundUp(std::size_t value, std::size_t multiple) {
        return (value + multiple - 1) / multiple * multiple;
    }

    // A block must fit a T or a FreeNode, and every block offset must satisfy
    // both alignments — hence size is rounded up to the stricter alignment.
    static constexpr std::size_t kBlockAlign = maxOf(alignof(T), alignof(FreeNode));
    static constexpr std::size_t kBlockSize =
        roundUp(maxOf(sizeof(T), sizeof(FreeNode)), kBlockAlign);

    void buildFreeList() {
        freeHead_ = nullptr;
        for (std::size_t i = capacity_; i > 0; --i) {  // head ends at block 0
            void* block = arena_ + (i - 1) * kBlockSize;
            freeHead_ = ::new (block) FreeNode{freeHead_};
        }
    }

    void releaseArena() noexcept {
        assert(inUse_ == 0 && "PoolAllocator: destroyed while objects are live");
        if (arena_ != nullptr) {
            ::operator delete(arena_, std::align_val_t{kBlockAlign});
            arena_ = nullptr;
        }
    }

    std::byte* arena_ = nullptr;
    FreeNode* freeHead_ = nullptr;
    std::size_t capacity_ = 0;
    std::size_t inUse_ = 0;
};

}  // namespace cppdb
