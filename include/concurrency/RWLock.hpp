#pragma once

#include <condition_variable>
#include <mutex>

namespace cppdb {

// Readers-writer lock: any number of concurrent readers OR one writer.
// Writer-preferring — once a writer is waiting, new readers queue behind it,
// so a read-heavy workload cannot starve writes (the reverse trade-off, a
// continuous write stream starving readers, is accepted; database workloads
// are read-heavy). Not recursive: re-locking from the same thread deadlocks.
class RWLock {
public:
    void readLock();
    void readUnlock();
    void writeLock();
    void writeUnlock();

private:
    std::mutex mutex_;
    std::condition_variable readersCv_;
    std::condition_variable writersCv_;
    int activeReaders_ = 0;
    int waitingWriters_ = 0;
    bool writerActive_ = false;
};

// RAII guards — the only way the rest of the codebase takes these locks, so
// an exception can never leak a held lock.
class ReadGuard {
public:
    explicit ReadGuard(RWLock& lock) : lock_(lock) { lock_.readLock(); }
    ~ReadGuard() { lock_.readUnlock(); }
    ReadGuard(const ReadGuard&) = delete;
    ReadGuard& operator=(const ReadGuard&) = delete;

private:
    RWLock& lock_;
};

class WriteGuard {
public:
    explicit WriteGuard(RWLock& lock) : lock_(lock) { lock_.writeLock(); }
    ~WriteGuard() { lock_.writeUnlock(); }
    WriteGuard(const WriteGuard&) = delete;
    WriteGuard& operator=(const WriteGuard&) = delete;

private:
    RWLock& lock_;
};

}  // namespace cppdb
