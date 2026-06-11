# `std::string`

**What:** A growable, owning sequence of characters. Manages its own heap memory (RAII) — no manual `malloc`/`free`, no null terminator bookkeeping.

**Why:** Replaces C's error-prone `char*`. Copies/moves/frees automatically with the object's lifetime.

**Example:**
```cpp
#include <string>
std::string s = "age";
s += "_col";                  // "age_col"
if (s == "age_col") { }       // value comparison (not pointer)
size_t n = s.size();
const char* c = s.c_str();    // C-API interop
```

**Used in:** column names, row values, query text — everywhere text appears.

**Gotcha:** Pass as `const std::string&` to avoid copies (see [passing-value-reference-constref](passing-value-reference-constref.md)). A `std::string` copy duplicates the characters on the heap — not free.
