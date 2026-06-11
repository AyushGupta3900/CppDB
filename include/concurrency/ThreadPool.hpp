#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace cppdb {

// Fixed-size pool of worker threads draining one shared task queue.
//
// Design note: the queue here is mutex + condition_variable, NOT the
// LockFreeQueue — that queue is single-consumer, while a pool has N consumer
// threads, and workers need a real blocking wait instead of a spin. The
// lock-free queue is used where its MPSC shape fits (see the TCP server).
//
// Destruction is graceful: already-submitted tasks finish, then workers join.
class ThreadPool {
public:
    // threadCount must be >= 1 (throws std::invalid_argument otherwise).
    explicit ThreadPool(std::size_t threadCount);

    // Blocks until every queued task has run, then joins all workers.
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Enqueue a task. Throws std::runtime_error if the pool is shutting down.
    // A task that throws is swallowed (a worker must never die); tasks that
    // care about errors should capture and report them themselves.
    void submit(std::function<void()> task);

    std::size_t threadCount() const noexcept { return workers_.size(); }

private:
    void workerLoop();

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stopping_ = false;
};

}  // namespace cppdb
