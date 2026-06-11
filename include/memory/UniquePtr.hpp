#pragma once

#include <cstddef>
#include <utility>

namespace cppdb {

// Move-only owning pointer (a teaching re-implementation of the core of
// std::unique_ptr). Owns a heap object allocated with `new`; deletes it in
// the destructor. Copying is deleted — exactly one owner at a time.
// No custom deleters and no array support (see tech-debt.md).
template <typename T>
class UniquePtr {
public:
    UniquePtr() noexcept = default;
    explicit UniquePtr(T* ptr) noexcept : ptr_(ptr) {}

    ~UniquePtr() { delete ptr_; }

    UniquePtr(const UniquePtr&) = delete;
    UniquePtr& operator=(const UniquePtr&) = delete;

    UniquePtr(UniquePtr&& other) noexcept : ptr_(other.release()) {}

    UniquePtr& operator=(UniquePtr&& other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    T& operator*() const noexcept { return *ptr_; }
    T* operator->() const noexcept { return ptr_; }
    T* get() const noexcept { return ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    // Gives up ownership without deleting.
    T* release() noexcept { return std::exchange(ptr_, nullptr); }

    // Deletes the current object (if any) and takes ownership of `ptr`.
    void reset(T* ptr = nullptr) noexcept {
        T* old = std::exchange(ptr_, ptr);
        delete old;
    }

    void swap(UniquePtr& other) noexcept { std::swap(ptr_, other.ptr_); }

    friend bool operator==(const UniquePtr& lhs, std::nullptr_t) noexcept {
        return lhs.ptr_ == nullptr;
    }

private:
    T* ptr_ = nullptr;
};

template <typename T, typename... Args>
UniquePtr<T> makeUnique(Args&&... args) {
    return UniquePtr<T>(new T(std::forward<Args>(args)...));
}

}  // namespace cppdb
