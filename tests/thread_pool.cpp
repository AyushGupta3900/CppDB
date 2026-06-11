#include "concurrency/ThreadPool.hpp"

#include <atomic>
#include <chrono>
#include <latch>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>

#include "test_framework.hpp"

using cppdb::ThreadPool;

static void testRunsEveryTask() {
    std::atomic<int> counter{0};
    constexpr int kTasks = 10'000;
    std::latch done(kTasks);
    {
        ThreadPool pool(4);
        CHECK_EQ(pool.threadCount(), 4u);
        for (int i = 0; i < kTasks; ++i) {
            pool.submit([&counter, &done] {
                counter.fetch_add(1);
                done.count_down();
            });
        }
        done.wait();
    }
    CHECK_EQ(counter.load(), kTasks);
}

static void testWorkRunsOnMultipleThreads() {
    std::mutex idsMutex;
    std::set<std::thread::id> ids;
    std::latch done(8);
    ThreadPool pool(4);
    for (int i = 0; i < 8; ++i) {
        pool.submit([&] {
            {
                std::lock_guard<std::mutex> guard(idsMutex);
                ids.insert(std::this_thread::get_id());
            }
            // Hold the task long enough that one worker cannot do them all.
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            done.count_down();
        });
    }
    done.wait();
    CHECK(ids.size() > 1u);
}

static void testDestructorDrainsQueue() {
    std::atomic<int> counter{0};
    {
        ThreadPool pool(2);
        for (int i = 0; i < 100; ++i) {
            pool.submit([&counter] {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                counter.fetch_add(1);
            });
        }
    }  // dtor must wait for all 100, not abandon the queue
    CHECK_EQ(counter.load(), 100);
}

static void testThrowingTaskDoesNotKillWorker() {
    std::atomic<int> counter{0};
    std::latch done(1);
    {
        ThreadPool pool(1);  // one worker: it must survive the throw
        pool.submit([] { throw std::runtime_error("task failed"); });
        pool.submit([&] {
            counter.fetch_add(1);
            done.count_down();
        });
        done.wait();
    }
    CHECK_EQ(counter.load(), 1);
}

static void testInvalidConstruction() {
    CHECK_THROWS(ThreadPool(0), std::invalid_argument);
}

int main() {
    testRunsEveryTask();
    testWorkRunsOnMultipleThreads();
    testDestructorDrainsQueue();
    testThrowingTaskDoesNotKillWorker();
    testInvalidConstruction();
    return testfw::testSummary("thread_pool");
}
