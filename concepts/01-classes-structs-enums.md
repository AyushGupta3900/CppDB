# Concept 01 — Classes, Structs, Enums & const-correctness

> Goal: build the `Schema` and `DataType`/`Column` types that describe a table's columns. Along the way, internalize the rules C++ uses for access control, value vs. reference, and `const`.

You're intermediate, so I'll skip "what is a class" and focus on the parts of C++ that bite people coming from Java/Python/JS.

---

## 1. `struct` vs `class` — there is almost no difference

In C++ they are **the same feature**. The *only* language difference:

- `struct` → members are **public** by default.
- `class` → members are **private** by default.

That's it. The real difference is **convention**:

- Use `struct` for **plain data bags** with no invariant to protect (e.g. `Column { name, type }` — any combination is valid).
- Use `class` when the object must **maintain an invariant** and therefore needs to control access (e.g. `Schema` — you can't let someone add two columns with the same name, so access goes through methods).

```cpp
struct Column {          // plain data — all public
    std::string name;
    DataType    type;
};

class Schema {           // has rules — private data, public methods
public:
    void addColumn(const std::string& name, DataType type);
private:
    std::vector<Column> columns_;   // trailing underscore = "private member" convention
};
```

The trailing underscore (`columns_`) is a widespread convention to distinguish members from locals/parameters. Pick it and stay consistent.

---

## 2. `enum class` — always prefer it over plain `enum`

A column's type is one of a fixed set: INT, TEXT, etc. That's an enumeration.

**Plain `enum` is dangerous** — its values leak into the surrounding scope and implicitly convert to `int`:

```cpp
enum Color { RED, GREEN };       // RED is now a global name
enum Fruit { APPLE, BANANA };    // ERROR if you'd also written RED here
int x = RED;                     // compiles silently — usually a bug
```

**`enum class` (scoped enum)** fixes both — values are namespaced and don't implicitly convert:

```cpp
enum class DataType { INT, TEXT, BOOL };

DataType t = DataType::INT;      // must qualify with DataType::
int x = t;                       // ERROR — no implicit conversion (good!)
int y = static_cast<int>(t);     // explicit conversion when you really mean it
```

**Rule: always `enum class`.** Plain `enum` is a legacy footgun.

---

## 3. Value vs reference vs pointer — the parameter-passing decision

This is the single biggest difference from Python/Java, where everything-object is a reference. In C++ **you choose**, and the choice has performance and correctness consequences.

```cpp
void f(std::string s);          // BY VALUE — copies the whole string. Caller's original untouched.
void f(std::string& s);         // BY REFERENCE — alias to caller's string. Can modify it.
void f(const std::string& s);   // BY CONST REFERENCE — alias, but read-only. No copy, no modify.
```

**The default you should reach for when passing objects you only read:** `const T&`.
- No copy (cheap, even for a huge string/vector).
- `const` promises the caller "I won't change your object."

Passing `std::string` (a potentially large object) **by value** copies every character. Passing `const std::string&` passes a hidden pointer — basically free.

> Exception: small, cheap-to-copy types (`int`, `double`, `DataType`, a pointer) — just pass by value. A reference to an `int` is the same size as the `int`, so there's no win.

---

## 4. `const` correctness — the compiler is your proof-checker

`const` is not decoration; it's a **machine-checked promise**. Three places it shows up here:

### (a) const parameters — "I won't modify your argument"
```cpp
void addColumn(const std::string& name, DataType type);
//                   ^^^^^ name cannot be modified inside the function
```

### (b) const member functions — "calling me won't change the object"
```cpp
class Schema {
public:
    bool hasColumn(const std::string& name) const;   // <-- const after the ()
    //                                          ^^^^^
};
```
A `const` member function promises not to modify any member. Inside it, every member is treated as `const`. **Why it matters:** if you have a `const Schema&` (very common — that's how you pass it around cheaply), you can *only* call its `const` methods. Forget the `const` on `hasColumn` and `const Schema` becomes nearly unusable.

> Rule of thumb: any method that only *reads* the object should be `const`. Mark them as you write them — retrofitting `const` later is painful.

### (c) const member variables / returning by const
We'll hit these in later lessons (the `const Schema&` member inside `Row` is a great trap — Lesson 2).

---

## 5. Header vs source — what goes where

- **`Schema.hpp`** (header): the class definition with member function **declarations** (signatures, ending in `;`). Plus `#pragma once` at the top and the `#include`s the *declarations* need (`<string>`, `<vector>`).
- **`Schema.cpp`** (source): the member function **definitions** (bodies), prefixed with `Schema::`.

```cpp
// Schema.hpp
#pragma once
#include <string>
#include <vector>

enum class DataType { INT, TEXT, BOOL };

struct Column { std::string name; DataType type; };

class Schema {
public:
    void addColumn(const std::string& name, DataType type);
    bool hasColumn(const std::string& name) const;
    size_t columnCount() const;
private:
    std::vector<Column> columns_;
};
```

```cpp
// Schema.cpp
#include "core/Schema.hpp"

void Schema::addColumn(const std::string& name, DataType type) {
    columns_.push_back(Column{name, type});   // Column{...} = aggregate init
}
// ... rest is YOUR job
```

See `../syntax/01-classes-structs-enums.md` for the compact patterns.

---

## ✅ Your Task — Lesson 1

Build the `Schema` type and its supporting types. **You write all the code.**

**Files to create:**
- `include/core/Schema.hpp`
- `src/core/Schema.cpp`
- `tests/schema.cpp`  (a `main()` that exercises Schema and prints PASS/FAIL)

**Requirements for `Schema`:**
1. An `enum class DataType` with at least `INT`, `TEXT`, `BOOL`.
2. A `struct Column` holding a `std::string name` and a `DataType type`.
3. `void addColumn(const std::string& name, DataType type)` — appends a column.
   - **Invariant:** reject (don't add) a column whose name already exists. Decide how to signal this — return `bool`? throw? Your call; be ready to defend it.
4. `bool hasColumn(const std::string& name) const` — true if a column with that name exists.
5. `DataType typeOf(const std::string& name) const` — return the type of a named column. Think about what to do if it doesn't exist.
6. `size_t columnCount() const` — number of columns.
7. Make **every read-only method `const`**.

**Your test (`tests/schema.cpp`) should at minimum:**
- Create a Schema, add a few columns.
- Verify `hasColumn` returns true for added, false for missing.
- Verify adding a duplicate name does **not** increase `columnCount()`.
- Print something like `all tests passed` at the end.

**Build & run:**
```bash
make build/test_schema && ./build/test_schema
```

When it compiles and runs, tell me and I'll review. Don't worry about getting it perfect — getting it *working*, then we polish.

### Stretch (optional, only if hungry)
- Add `std::string typeName(DataType)` free function that returns `"INT"`/`"TEXT"`/`"BOOL"` (teaches `switch` over `enum class`).
