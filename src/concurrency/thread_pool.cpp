#include "concurrency/ThreadPool.hpp"

#include <stdexcept>
#include <utility>

namespace cppdb {

ThreadPool::ThreadPool(std::size_t threadCount) {
    if (threadCount == 0) {
        throw std::invalid_argument("ThreadPool: threadCount must be >= 1");
    }
    workers_.reserve(threadCount);
    for (std::size_t i = 0; i < threadCount; ++i) {
        workers_.emplace_back([this] { workerLoop(); });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> guard(mutex_);
        stopping_ = true;
    }
    cv_.notify_all();
    for (std::thread& worker : workers_) {
        worker.join();
    }
}

void ThreadPool::submit(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (stopping_) {
            throw std::runtime_error("ThreadPool: submit after shutdown");
        }
        tasks_.push(std::move(task));
    }
    cv_.notify_one();
}

void ThreadPool::workerLoop() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> guard(mutex_);
            cv_.wait(guard, [this] { return stopping_ || !tasks_.empty(); });
            if (tasks_.empty()) return;  // stopping_ && drained -> exit
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        try {
            task();  // run outside the lock so tasks execute in parallel
        } catch (...) {
            // Swallow: a throwing task must not kill the worker thread.
        }
    }
}

}  // namespace cppdb
