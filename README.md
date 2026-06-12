# CppDB

A thread-safe, in-memory relational database engine written from scratch in
**C++23** — no third-party libraries, no STL containers on the hot path. Every
load-bearing data structure (hash map, B-tree, LRU cache), the memory
subsystem (pool allocator, smart pointers), the concurrency layer
(readers-writer lock, lock-free queue, thread pool), the SQL front-end
(lexer → parser → executor), and the network layer (kqueue event loop) is
hand-built and unit-tested.

```
$ ./build/cppdb 5432
CppDB listening on 127.0.0.1:5432  (connect with: nc localhost 5432)

$ nc localhost 5432
CREATE TABLE users (id INT, name TEXT, age INT)
OK
INSERT INTO users (id, name, age) VALUES (1, 'Alice', 30)
OK
SELECT * FROM users WHERE id = 1
id=1, name=Alice, age=30
(1 row)
UPDATE users SET age = 31 WHERE name = 'Alice'
OK (1 row updated)
DELETE FROM users WHERE age >= 31
OK (1 row deleted)
```

## Quickstart

Requires clang with C++23 (tested with Apple clang 21) and `make`. macOS/BSD
only — the server uses `kqueue`.

```bash
make                  # build the server -> ./build/cppdb [port]
make check            # build + run the full test suite (428k+ checks)
make tsan             # the same suite under ThreadSanitizer
make asan             # ... and under AddressSanitizer + UBSan
make bench            # optimized benchmarks (see docs/BENCHMARKS.md)
```

## Architecture

```
            ┌────────────────────────────────────────────────┐
  nc/telnet │  TCPServer  (kqueue event loop, non-blocking)  │  network/
            │   └─ per-connection strand on the ThreadPool   │
            └────────────────────┬───────────────────────────┘
                                 │ one query per line
            ┌────────────────────▼───────────────────────────┐
            │  QueryExecutor ◄─ Parser ◄─ Lexer               │  query/
            │  (AST variant; WHERE id ... uses the index)     │
            └────────────────────┬───────────────────────────┘
                                 │
            ┌────────────────────▼───────────────────────────┐
            │  Database ── SharedPtr<Table> ── Schema/Row     │  core/
            │   Table: RWLock ─ BTree<id,Row> ─ LRUCache      │
            └────────────────────┬───────────────────────────┘
                                 │ built on
            ┌────────────────────▼───────────────────────────┐
            │  structures/  HashMap · BTree · LRUCache        │
            │  memory/      UniquePtr · SharedPtr             │
            │  storage/     PoolAllocator                     │
            │  concurrency/ RWLock · ThreadPool · MPSC queue  │
            └────────────────────────────────────────────────┘
```

Key design decisions, and the trade-offs behind them:

- **Storage is a B-tree clustered on the `id` primary key** — O(log n) point
  lookups, scans in id order for free, and `WHERE id < n` answered by a pruned
  range scan instead of a full pass.
- **Reads return copies, never pointers into storage.** The tree rebalances
  and other threads write; a pointer would be a use-after-move waiting to
  happen. Hot reads are absorbed by a per-table LRU cache (invalidated on
  every write to that id).
- **One RWLock per table, writer-preferring** — parallel `SELECT`s, exclusive
  writes, and a steady write stream cannot be starved by readers.
- **Tables are held by `SharedPtr` (our own)** — a handle returned by
  `getTable` stays valid even if another thread drops the table a microsecond
  later.
- **The thread pool deliberately does *not* use the lock-free queue.** The
  MPSC queue has a single-consumer contract; a pool has N consumers and needs
  a blocking wait. Each is used where its shape fits.
- **The executor never throws** — every malformed query becomes an
  `ERROR: ...` reply, because a network server must outlive its worst client.
  Connections are also bounded (64 KB lines, 1024 pipelined queries) so a
  hostile client cannot grow server memory.

## SQL surface

`CREATE TABLE` / `DROP TABLE` / `INSERT` / `SELECT` (projection, `*`) /
`UPDATE ... SET` / `DELETE`, with `WHERE col op literal [AND ...]` where `op`
is `= != < <= > >=`. Strings escape quotes SQL-style (`'O''Brien'`). Every
table requires an `INT id` primary key; updating it is rejected.

## Verification

| Gate | Status |
|---|---|
| `make check` — 14 suites, 428k+ assertions | passing |
| `make tsan` — full suite under ThreadSanitizer | clean |
| `make asan` — full suite under ASan + UBSan | clean |
| `-Wall -Wextra -Wpedantic` (CI adds `-Werror`) | warning-free |
| Randomized model tests (B-tree vs `std::set`, LRU vs reference) | passing |

Benchmarks (Apple M2, `-O2`): pool allocator **1.9 ns/op (~24× over
`new/delete`)** · B-tree lookup **271 ns (~63× over a scan at 100k rows)** ·
LRU **89.8% hit rate @ 8.2M reads/s** · MPSC queue **12.7M items/s** · TCP
**50 concurrent clients @ ~56k queries/s**. Details and caveats in
[docs/BENCHMARKS.md](docs/BENCHMARKS.md).

## Repository layout

```
include/, src/    the engine (core, structures, memory, storage,
                  concurrency, query, network) + main.cpp
tests/            one binary per module + a tiny header-only test framework
bench/            -O2 micro-benchmarks behind `make bench`
docs/             architecture, build model, benchmarks, progress
concepts/, syntax/, cpp-concepts/   learning notes the project grew from
tech-debt.md      every shortcut, why, and how to pay it back
```

This project began as a guided learning build (see [STARTER.md](STARTER.md)
and [docs/](docs/README.md)); the history is a readable sequence of
conventional commits, one per milestone.
