# CppDB — Learning Documentation

A from-scratch, thread-safe in-memory database built in C++ to learn the language **deeply** — OOP, memory management, templates, concurrency, data structures, parsing, and networking.

This `docs/` folder is the **map**. The actual learning happens across three folders:

| Folder        | What lives here                                                                 |
|---------------|---------------------------------------------------------------------------------|
| `concepts/`   | **The "why".** Deep explanations of each C++ concept, why it exists, when to use it, and the traps. Read these first. |
| `syntax/`     | **The "how".** Compact syntax references for each feature — copy-paste-able patterns you'll forget and re-look-up. |
| `docs/`       | **The "what".** Project overview, architecture, build instructions, and progress tracking. |
| `include/` `src/` `tests/` | **Your code.** You write these. I review and hint. |

## How we work

1. I write a **concept lesson** (`concepts/`) + a **syntax sheet** (`syntax/`).
2. I give you a **task** with exact files to create and a checklist of requirements.
3. **You write the code** in `include/` and `src/`, plus a small test in `tests/`.
4. You run it: `make build/test_<name> && ./build/test_<name>`.
5. I **review** your code, point out bugs/idioms/improvements, and give hints (not full solutions unless you ask).
6. We move to the next concept.

## The 7 stages

| Stage | Concepts |
|-------|----------|
| 1 | OOP, const-correctness, Templates, Operator Overloading, **Rule of Five** |
| 2 | Memory Management, Pool Allocator, RAII, Smart Pointers, placement `new` |
| 3 | Threads, Mutex, RWLock, Condition Variables, Atomics, lock-free |
| 4 | B-Tree Index, recursive pointer structures, node splitting |
| 5 | LRU Cache (intrusive list + hashmap) |
| 6 | Query Parsing, Lexer, AST, basic compiler frontend |
| 7 | TCP Server, sockets, non-blocking I/O |

See [STARTER.md](../STARTER.md) for the full class blueprints, and [PROGRESS.md](PROGRESS.md) for where we are.

## Build

You have **Apple clang 21** (C++23) and **make**. No CMake needed.

```bash
make tests                              # build all test binaries
make build/test_schema                  # build one test
./build/test_schema                     # run it
make                                    # build the main app (later)
make clean                              # remove build artifacts
```

See [BUILD.md](BUILD.md) for details on the compilation model.
