# Resume Description — CppDB

> Use only the bullets for features you actually built. Replace every `[…]` with a **real measured number** (run a benchmark first). Interviewers will drill into anything you list — every line here maps to code you wrote and can explain.

---

## Project header (top line)

**CppDB — Thread-Safe In-Memory Database Engine** · *C++23, STL, POSIX threads/sockets*
Personal systems project · [github.com/you/CppDB] · [Month YYYY]

---

## Short version (3 bullets — for a dense, multi-project resume)

- Built a **thread-safe in-memory database engine in modern C++ (C++23)** from scratch — custom hash map, B-tree primary index, LRU cache, pool allocator, and a SQL-subset query parser — totaling **~2,900 lines** across **7 modules** (core, structures, memory, storage, concurrency, query, network) plus **~2,200 lines** of unit tests and benchmarks.
- Implemented a **readers-writer locking layer and a fixed-size thread pool** enabling concurrent reads with exclusive writes, sustaining **50 concurrent client connections at ~56,000 queries/sec** end-to-end over a TCP server built on non-blocking I/O (kqueue).
- Designed a **custom memory subsystem** — pool allocator with placement `new`, plus hand-rolled `unique_ptr`/`shared_ptr` — cutting per-allocation cost from **45 ns to 1.9 ns (~24×)** vs. raw `new`/`delete` and demonstrating manual RAII and reference counting.

---

## Detailed version (6–7 bullets — for a focused / new-grad resume)

- **Architected a mini relational database engine in C++23** (Redis/SQLite-inspired) supporting `CREATE / INSERT / SELECT / UPDATE / DELETE` with multi-condition `WHERE ... AND`, organized into decoupled modules: storage, indexing, concurrency, query parsing, and networking — CI-gated with the full suite run under ThreadSanitizer **and** AddressSanitizer/UBSan.
- **Built core data structures from first principles** instead of using the STL containers: an open-addressing **hash map**, a **B-tree** primary-key index reducing lookups from O(n) to **O(log n)** (measured **63× faster** than a scan at 100k rows), and an **LRU cache** (linked list + hash map) giving **O(1)** get/put — **89.8% hit rate at 8.2M reads/sec** on a skewed workload.
- **Engineered a custom memory-management layer** — a fixed-block **pool allocator** using placement `new` over a raw memory arena (**~24× faster** than `new`/`delete`), and from-scratch **smart pointers** (`UniquePtr` move-only, `SharedPtr` with atomic reference counting) — to internalize RAII, ownership, and the Rule of Five.
- **Implemented a concurrency layer for safe multi-client access**: a **readers-writer lock** (parallel reads, exclusive writes) built on `std::mutex` + `std::condition_variable`, a **lock-free MPSC queue** using `std::atomic` (**12.7M items/sec**, 4 producers), and a **thread pool** dispatching queries across **4** worker threads — verified race- and deadlock-free under ThreadSanitizer.
- **Wrote a query front-end (compiler basics)**: a hand-written **lexer/tokenizer** and recursive-descent **parser** producing an AST, plus an **executor** that walks the AST against the storage engine — turning raw SQL-like strings into table operations.
- **Exposed the database over the network** via a **TCP server** using non-blocking I/O (`kqueue`) and newline-delimited framing with per-connection ordering, letting clients connect over a socket (`nc`/`telnet`) and run queries remotely — **50 concurrent clients, ~56k queries/sec** end-to-end.
- **Practiced production-grade C++ discipline**: compiled `-Wall -Wextra -Wpedantic` warning-clean, applied `const`-correctness and move semantics throughout, and backed each module with unit tests and a reproducible build.

---

## One-liner (LinkedIn headline / portfolio subtitle)

> Built a thread-safe in-memory database engine in C++23 — custom allocator, B-tree index, lock-free concurrency, SQL parser, and a TCP server — to master systems-level C++.

---

## Skills / keywords line (for an ATS skills section)

`C++23 · STL · Memory Management · RAII · Smart Pointers · Templates · Multithreading · std::atomic · Lock-Free Programming · Readers-Writer Locks · Thread Pools · Data Structures (B-Tree, Hash Map, LRU) · Pool Allocator · Compiler/Parser Design · Socket Programming · epoll · Concurrency · Systems Programming`

---

## Talking points to prepare (interviewers WILL ask)

For each line above, be ready to explain **the why and a trade-off**:
- **Rule of Five** — why move semantics matter; what a shallow copy would break in `Row`.
- **Pool allocator** — why fixed-size blocks beat general `malloc` for uniform `Row` objects; fragmentation.
- **SharedPtr ref count** — why the counter must be `atomic` for thread safety; cycles → leaks.
- **RWLock** — reader/writer starvation; why a plain mutex would serialize all reads.
- **Lock-free queue** — what CAS is, the ABA problem, why `compare_exchange_weak`.
- **B-tree vs BST vs hash** — why B-tree (cache locality, range scans) for an index.
- **Iterator invalidation** — why the LRU pairs a list with a map of iterators.

> Rule: if you can't explain a bullet for 2 minutes at a whiteboard, **cut it** until you can.

---

## Metrics to actually measure (so the `[…]` become real)

| Placeholder            | How to get a real number                                                |
|------------------------|-------------------------------------------------------------------------|
| lines of code / modules| `cloc .` or `find src include -name '*.?pp' \| xargs wc -l`              |
| allocation speedup     | micro-benchmark: N pool allocs vs N `new`/`delete`, `std::chrono`        |
| lookup latency improve | time M random lookups: linear scan vs B-tree index                       |
| LRU hit-rate / latency | run a skewed (Zipfian) key workload, measure cache hit %                 |
| concurrent connections | load-test the TCP server with a script opening K sockets                 |
| thread-pool throughput | queries/sec at 1, 2, 4, 8 worker threads                                 |
