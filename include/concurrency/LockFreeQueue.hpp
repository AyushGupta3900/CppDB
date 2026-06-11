#pragma once

#include <atomic>
#include <optional>
#include <utility>

namespace cppdb {

// Lock-free multi-producer single-consumer (MPSC) FIFO queue.
//
// Shape: an intrusive singly-linked list with a permanent stub node at the
// head. Producers append in two steps — (1) atomically swing tail_ to the new
// node, (2) link the previous tail to it. Step 1 makes push wait-free with
// respect to other producers: no CAS retry loop, every producer wins exactly
// one slot in the chain. Between (1) and (2) the chain is momentarily
// unlinked; the consumer simply sees "nothing ready yet" and returns false,
// which is safe for a task queue.
//
// CONTRACT: any number of threads may push(); exactly ONE thread may call
// pop()/empty(). head_ is deliberately non-atomic — it is consumer-private.
template <typename T>
class LockFreeQueue {
public:
    LockFreeQueue() {
        Node* stub = new Node();
        head_ = stub;
        tail_.store(stub, std::memory_order_relaxed);
    }

    ~LockFreeQueue() {
        Node* node = head_;
        while (node != nullptr) {
            Node* next = node->next.load(std::memory_order_relaxed);
            delete node;
            node = next;
        }
    }

    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;

    // Multi-producer safe.
    void push(T item) {
        Node* node = new Node(std::move(item));
        // acq_rel: release publishes the node's payload to the consumer,
        // acquire orders us after the previous producer's exchange.
        Node* prev = tail_.exchange(node, std::memory_order_acq_rel);
        prev->next.store(node, std::memory_order_release);
    }

    // Single consumer only. Returns false when nothing is ready.
    bool pop(T& out) {
        Node* next = head_->next.load(std::memory_order_acquire);
        if (next == nullptr) return false;
        out = std::move(*next->value);
        next->value.reset();
        delete head_;
        head_ = next;  // the popped node becomes the new stub
        return true;
    }

    // Single consumer only (same thread as pop).
    bool empty() const {
        return head_->next.load(std::memory_order_acquire) == nullptr;
    }

private:
    struct Node {
        std::optional<T> value;  // empty in the stub node
        std::atomic<Node*> next{nullptr};

        Node() = default;
        explicit Node(T v) : value(std::move(v)) {}
    };

    Node* head_;               // consumer-private stub
    std::atomic<Node*> tail_;  // producers contend here
};

}  // namespace cppdb
