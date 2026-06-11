# `std::vector<T>`

**What:** A dynamic array — contiguous, growable, owns its elements. The default container in C++.

**Why:** Contiguous memory = cache-friendly and fast to iterate. Manages its own storage (RAII): grows on `push_back`, frees on destruction.

**Example:**
```cpp
#include <vector>
std::vector<Column> cols;
cols.push_back(Column{"id", DataType::INT});  // append (may reallocate)
cols.size();                                   // count
cols[0].name;                                  // index access (no bounds check)
cols.at(0);                                    // bounds-checked (throws out_of_range)
for (const Column& c : cols) { /* ... */ }     // iterate by const-ref
```

**Used in:** `Schema::columns_`, token lists, B-tree node keys/values.

**Gotcha:** `push_back` may **reallocate** and move all elements → any pointers/references/iterators into the vector become **invalid** ("iterator invalidation"). Big topic in Stage 5. `reserve(n)` pre-allocates to avoid repeated reallocations.
