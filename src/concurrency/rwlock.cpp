#include "concurrency/RWLock.hpp"

namespace cppdb {

void RWLock::readLock() {
    std::unique_lock<std::mutex> guard(mutex_);
    // Queue behind waiting writers, not just the active one (writer preference).
    readersCv_.wait(guard, [this] { return !writerActive_ && waitingWriters_ == 0; });
    ++activeReaders_;
}

void RWLock::readUnlock() {
    std::unique_lock<std::mutex> guard(mutex_);
    --activeReaders_;
    if (activeReaders_ == 0) {
        writersCv_.notify_one();
    }
}

void RWLock::writeLock() {
    std::unique_lock<std::mutex> guard(mutex_);
    ++waitingWriters_;
    writersCv_.wait(guard, [this] { return !writerActive_ && activeReaders_ == 0; });
    --waitingWriters_;
    writerActive_ = true;
}

void RWLock::writeUnlock() {
    std::unique_lock<std::mutex> guard(mutex_);
    writerActive_ = false;
    if (waitingWriters_ > 0) {
        writersCv_.notify_one();   // hand off to the next writer first
    } else {
        readersCv_.notify_all();   // otherwise release the whole reader herd
    }
}

}  // namespace cppdb
