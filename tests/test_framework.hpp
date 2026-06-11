#pragma once

// Minimal header-only test framework: CHECK macros + a summary.
// Each tests/*.cpp is its own binary; call testSummary() at the end of main
// and return its result so `make check` can detect failures via exit code.

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

namespace testfw {

// Atomic so CHECK can be used from concurrency tests' worker threads.
inline std::atomic<int>& failures() {
    static std::atomic<int> count{0};
    return count;
}

inline std::atomic<int>& checks() {
    static std::atomic<int> count{0};
    return count;
}

inline void reportFailure(const char* file, int line, const std::string& message) {
    ++failures();
    std::cerr << "FAIL " << file << ":" << line << "  " << message << "\n";
}

inline int testSummary(const std::string& suiteName) {
    if (failures().load() == 0) {
        std::cout << "[" << suiteName << "] " << checks().load() << " checks passed\n";
        return EXIT_SUCCESS;
    }
    std::cerr << "[" << suiteName << "] " << failures().load() << "/" << checks().load()
              << " checks FAILED\n";
    return EXIT_FAILURE;
}

}  // namespace testfw

#define CHECK(cond)                                                     \
    do {                                                                \
        ++testfw::checks();                                             \
        if (!(cond)) {                                                  \
            testfw::reportFailure(__FILE__, __LINE__, "CHECK(" #cond ")"); \
        }                                                               \
    } while (0)

// Capture by value: `actual` may be a reference into a temporary (e.g.
// findById(1)->get("name")) that dies at the end of the declaration.
#define CHECK_EQ(actual, expected)                                      \
    do {                                                                \
        ++testfw::checks();                                             \
        const auto a_ = (actual);                                       \
        const auto e_ = (expected);                                     \
        if (!(a_ == e_)) {                                              \
            std::ostringstream oss_;                                    \
            oss_ << "CHECK_EQ(" #actual ", " #expected ")  got: " << a_ \
                 << "  want: " << e_;                                   \
            testfw::reportFailure(__FILE__, __LINE__, oss_.str());      \
        }                                                               \
    } while (0)

#define CHECK_THROWS(expr, ExceptionType)                               \
    do {                                                                \
        ++testfw::checks();                                             \
        bool caught_ = false;                                           \
        try {                                                           \
            (void)(expr);                                               \
        } catch (const ExceptionType&) {                                \
            caught_ = true;                                             \
        } catch (...) {                                                 \
        }                                                               \
        if (!caught_) {                                                 \
            testfw::reportFailure(__FILE__, __LINE__,                   \
                                  "CHECK_THROWS(" #expr ", " #ExceptionType ") did not throw"); \
        }                                                               \
    } while (0)
