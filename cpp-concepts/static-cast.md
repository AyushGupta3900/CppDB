# `static_cast<T>`

**What:** A compile-time, checked conversion between related types (numeric conversions, `enum class` ↔ int, up/down a known class hierarchy, `void*` → typed pointer).

**Why:** C++ deliberately removes many *implicit* conversions for safety. `static_cast` is how you say "yes, I really mean this conversion" — and it's greppable, unlike C-style `(int)x`.

**Example:**
```cpp
DataType t = DataType::TEXT;
int n = static_cast<int>(t);          // 1

double d = 3.9;
int i = static_cast<int>(d);          // 3 (truncates)
```

**Used in:** converting `DataType` to an index/int when needed; Stage 2 pool allocator casts raw `void*` to `T*`.

**Gotcha:** `static_cast` is checked at compile time only. Use `reinterpret_cast` for raw byte reinterpretation (rare, dangerous), `dynamic_cast` for runtime-checked polymorphic downcasts. Prefer the *narrowest* cast that works.
