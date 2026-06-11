# `assert`

**What:** A macro from `<cassert>` that aborts the program (with file/line) if its condition is false. A self-check baked into the code.

**Why:** Cheap way to verify assumptions and write quick tests without a framework. Documents what *must* be true at a point.

**Example:**
```cpp
#include <cassert>
Schema s;
s.addColumn("id", DataType::INT);
assert(s.hasColumn("id"));        // if false → "Assertion failed" + abort
assert(s.columnCount() == 1);
```

**Used in:** the `tests/*.cpp` checks until we (optionally) add a real test framework.

**Gotcha:** Asserts are **compiled out** when `NDEBUG` is defined (release builds). So never put code with *side effects* inside an assert — `assert(doImportantThing())` would vanish in release. Use them for checks, not logic.
