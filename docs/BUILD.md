# Build & Compilation Model

## TL;DR

```bash
make build/test_schema && ./build/test_schema   # build + run one test
make tests                                      # build every tests/*.cpp
make clean                                      # wipe build/
```

## How C++ compilation works (the mental model)

C++ doesn't compile your whole program at once. Each `.cpp` file (a **translation unit**) is compiled **independently** into an object file (`.o`), then a **linker** stitches them together.

```
Schema.cpp  ──compile──▶  Schema.o  ─┐
Row.cpp     ──compile──▶  Row.o     ─┼──link──▶  test_schema (executable)
test_schema.cpp ─compile▶ test.o    ─┘
```

This is why we split declarations and definitions:

- **`include/.../Schema.hpp`** — the **declaration** (the "what"): class shape, function signatures. `#include`d by anyone who wants to *use* Schema.
- **`src/.../Schema.cpp`** — the **definition** (the "how"): the actual function bodies. Compiled once.

When `test_schema.cpp` does `#include "core/Schema.hpp"`, the compiler learns Schema *exists* and what its methods look like — enough to type-check the calls. The linker later resolves those calls to the real code compiled from `Schema.cpp`. If a definition is missing, you get an **"undefined reference" linker error** (not a compile error) — a classic C++ gotcha.

## Why header guards / `#pragma once`

If two files both `#include "Schema.hpp"`, the class would be defined twice in one translation unit → error. `#pragma once` (top of every header) tells the compiler "include me at most once per translation unit."

## Our flags

`-std=c++23` modern C++ · `-Wall -Wextra -Wpedantic` maximum warnings (treat them as bugs) · `-g` debug symbols · `-Iinclude` lets you write `#include "core/Schema.hpp"` instead of relative paths · `-pthread` for Stage 3 threading.

> Turn warnings into a habit: if the compiler warns, fix it before moving on. Most C++ bugs announce themselves as warnings first.
