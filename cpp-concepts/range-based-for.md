# Range-based `for`

**What:** `for (decl : container)` iterates every element without manual indices or iterators.

**Why:** Cleaner and less error-prone than index loops; works on any type with `begin()`/`end()` (vectors, strings, maps, your own classes later).

**Example:**
```cpp
for (const Column& c : columns_)  { /* read-only, no copy  */ }
for (Column& c : columns_)        { c.name += "!"; /* modify */ }
for (auto& c : columns_)          { /* deduce type, still ref */ }
for (Column c : columns_)         { /* COPY each element — usually a mistake */ }
```

**Used in:** scanning columns, tokens, rows.

**Gotcha:** Forgetting `&` copies every element. Default to `const auto&` for reads, `auto&` to modify. Don't `push_back`/`erase` on the container you're ranging over — it can invalidate the loop.
