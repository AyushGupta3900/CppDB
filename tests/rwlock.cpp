#include "concurrency/RWLock.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "test_framework.hpp"

using cppdb::ReadGuard;
using cppdb::RWLock;
using cppdb::WriteGuard;

namespace {

constexpr auto kShortNap = std::chrono::milliseconds(20);

}  // namespace

// Readers must be able to hold the lock at the same time.
static void testReadersRunInParallel() {
    RWLock lock;
    std::atomic<int> insideNow{0};
    std::atomic<int> maxInside{0};
    const int readerCount = 4;

    std::vector<std::thread> readers;
    for (int i = 0; i < readerCount; ++i) {
        readers.emplace_back([&] {
            ReadGuard guard(lock);
            const int now = insideNow.fetch_add(1) + 1;
            int seenMax = maxInside.load();
            while (seenMax < now && !maxInside.compare_exchange_weak(seenMax, now)) {
            }
            std::this_thread::sleep_for(kShortNap);  // keep the section open
            insideNow.fetch_sub(1);
        });
    }
    for (auto& t : readers) t.join();

    // All four readers sleep 20ms inside the lock; if reads serialized, only
    // one could ever be inside at a time.
    CHECK(maxInside.load() > 1);
}

// A writer must be alone: no reader and no other writer inside.
static void testWriterExclusivity() {
    RWLock lock;
    int unprotectedCounter = 0;  // plain int: races would corrupt it (and TSan would bark)
    std::atomic<int> readersInsideWrite{0};
    std::atomic<bool> sawOverlap{false};

    const int writerThreads = 4;
    const int readerThreads = 4;
    const int incrementsPerWriter = 25'000;

    std::vector<std::thread> threads;
    for (int w = 0; w < writerThreads; ++w) {
        threads.emplace_back([&] {
            for (int i = 0; i < incrementsPerWriter; ++i) {
                WriteGuard guard(lock);
                if (readersInsideWrite.load() != 0) sawOverlap.store(true);
                ++unprotectedCounter;
            }
        });
    }
    for (int r = 0; r < readerThreads; ++r) {
        threads.emplace_back([&] {
            for (int i = 0; i < incrementsPerWriter; ++i) {
                ReadGuard guard(lock);
                readersInsideWrite.fetch_add(1);
                (void)unprotectedCounter;
                readersInsideWrite.fetch_sub(1);
            }
        });
    }
    for (auto& t : threads) t.join();

    CHECK(!sawOverlap.load());
    CHECK_EQ(unprotectedCounter, writerThreads * incrementsPerWriter);
}

// Writer preference: while a writer waits, newly arriving readers must queue
// behind it instead of overtaking forever.
static void testWaitingWriterBlocksNewReaders() {
    RWLock lock;
    std::atomic<bool> writerDone{false};
    std::atomic<bool> lateReaderRanBeforeWriter{false};

    // Reader 1 holds the lock so the writer has to wait.
    std::thread holder([&] {
        ReadGuard guard(lock);
        std::this_thread::sleep_for(kShortNap * 3);
    });
    std::this_thread::sleep_for(kShortNap);  // let the holder acquire

    std::thread writer([&] {
        WriteGuard guard(lock);
        writerDone.store(true);
    });
    std::this_thread::sleep_for(kShortNap);  // let the writer start waiting

    std::thread lateReader([&] {
        ReadGuard guard(lock);
        if (!writerDone.load()) lateReaderRanBeforeWriter.store(true);
    });

    holder.join();
    writer.join();
    lateReader.join();
    CHECK(!lateReaderRanBeforeWriter.load());
}

int main() {
    testReadersRunInParallel();
    testWriterExclusivity();
    testWaitingWriterBlocksNewReaders();
    return testfw::testSummary("rwlock");
}
