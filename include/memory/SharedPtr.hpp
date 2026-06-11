#pragma once

#include <atomic>
#include <cstddef>
#include <utility>

namespace cppdb {

// Reference-counted owning pointer (a teaching re-implementation of the core
// of std::shared_ptr). The count lives in a heap control block shared by all
// copies and is atomic, so copies may be made and dropped from different
// threads safely. (Mutating the *same* SharedPtr instance from two threads is
// not safe — same contract as std::shared_ptr.)
// No weak_ptr / aliasing / custom deleters (see tech-debt.md).
template <typename T>
class SharedPtr {
public:
    SharedPtr() noexcept = default;

    explicit SharedPtr(T* ptr) : ptr_(ptr) {
        if (ptr_ != nullptr) {
            control_ = new ControlBlock{};  // starts at 1
        }
    }

    SharedPtr(const SharedPtr& other) noexcept
        : ptr_(other.ptr_), control_(other.control_) {
        if (control_ != nullptr) {
            // Relaxed is enough: we already own a reference through `other`,
            // so the count cannot reach zero concurrently.
            control_->strong.fetch_add(1, std::memory_order_relaxed);
        }
    }

    SharedPtr(SharedPtr&& other) noexcept
        : ptr_(std::exchange(other.ptr_, nullptr)),
          control_(std::exchange(other.control_, nullptr)) {}

    SharedPtr& operator=(const SharedPtr& other) noexcept {
        SharedPtr copy(other);  // copy-and-swap handles self-assignment
        swap(copy);
        return *this;
    }

    SharedPtr& operator=(SharedPtr&& other) noexcept {
        SharedPtr stolen(std::move(other));
        swap(stolen);
        return *this;
    }

    ~SharedPtr() { release(); }

    long useCount() const noexcept {
        return control_ == nullptr ? 0
                                   : control_->strong.load(std::memory_order_relaxed);
    }

    T* get() const noexcept { return ptr_; }
    T& operator*() const noexcept { return *ptr_; }
    T* operator->() const noexcept { return ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    void reset() noexcept {
        release();
        ptr_ = nullptr;
        control_ = nullptr;
    }

    void reset(T* ptr) {
        SharedPtr fresh(ptr);
        swap(fresh);
    }

    void swap(SharedPtr& other) noexcept {
        std::swap(ptr_, other.ptr_);
        std::swap(control_, other.control_);
    }

private:
    struct ControlBlock {
        std::atomic<long> strong{1};
    };

    // Drop one reference; the thread that takes the count to zero deletes.
    // acq_rel: the release half publishes our writes to the object, the
    // acquire half (on the final decrement) sees every other thread's writes
    // before the delete.
    void release() noexcept {
        if (control_ != nullptr &&
            control_->strong.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            delete ptr_;
            delete control_;
        }
    }

    T* ptr_ = nullptr;
    ControlBlock* control_ = nullptr;
};

template <typename T, typename... Args>
SharedPtr<T> makeShared(Args&&... args) {
    return SharedPtr<T>(new T(std::forward<Args>(args)...));
}

}  // namespace cppdb
