# `enum class` (scoped enum)

**What:** A type-safe enumeration whose values are namespaced under the enum's name and do **not** implicitly convert to `int`.

**Why:** Plain `enum` leaks its names into the surrounding scope and silently converts to `int` — two classic bug sources. `enum class` fixes both.

**Example:**
```cpp
enum class DataType { INT, TEXT, BOOL };

DataType t = DataType::INT;        // must qualify
int bad = t;                       // ERROR (good — no implicit conversion)
int n   = static_cast<int>(t);     // explicit when you truly need the number
```

**Used in:** `DataType`; later `TokenType` in the query lexer.

**Gotcha:** You **must** write `DataType::INT`, not bare `INT`. Underlying type is `int` by default; you can pick another: `enum class Flags : uint8_t { ... }`.
