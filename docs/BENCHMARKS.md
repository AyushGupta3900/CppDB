# Benchmarks

Measured on Apple M2 (arm64), Apple clang 21, `-O2 -DNDEBUG`. Reproduce with:

```bash
make bench
```

Source: [bench/bench.cpp](../bench/bench.cpp). Run date: 2026-06-11.

| Benchmark | Result |
|---|---|
| **PoolAllocator** — 1M alloc+free of 64-byte blocks | `new`/`delete` 45.3 ns/op → pool **1.9 ns/op (~24× faster)** |
| **B-tree index** — point lookups among 100k rows | linear scan 17.1 µs → B-tree **271 ns (~63× faster, O(n) → O(log n))** |
| **LRU row cache** — 200k skewed reads (90% on 500 hot ids), capacity 1000 | **89.8% hit rate**, 8.2M reads/sec |
| **ThreadPool** — 200k no-op tasks | 1 worker 27.3M/s · 2 workers 6.9M/s · 4 workers 5.1M/s · 8 workers 1.0M/s |
| **LockFreeQueue** — 4 producers → 1 consumer | **12.7M items/sec** |
| **TCP server** — 50 concurrent clients × 20 INSERTs | **1000/1000 completed, ~56,500 queries/sec** end-to-end over loopback |

## Reading the numbers honestly

- **Pool allocator** wins because every alloc/free is a pointer pop/push on a
  pre-faulted arena — no size-class lookup, no syscalls, no fragmentation. The
  trade-off: fixed capacity and one object type per pool.
- **ThreadPool throughput *decreases* with workers on no-op tasks.** That is
  expected, not a bug: a no-op task measures pure queue contention — every
  worker fights for the same mutex and there is no work to parallelize. With
  real work per task (queries, I/O), added workers pay off; the TCP benchmark
  (4 workers, real parsing + B-tree writes) shows the pool earning its keep.
- **LRU hit rate ≈ the workload's skew** (90% hot traffic → 89.8% hits), which
  is exactly what a correctly-evicting LRU should converge to when the hot set
  fits in capacity.
- The TCP number includes the full stack per query: kqueue wakeup → line
  framing → lex → parse → execute under the table's write lock → reply write.
