# Passing by Value / Reference / const-Reference

**What:** In C++ *you* decide how an argument is passed. The choice affects copies (performance) and whether the callee can modify the caller's object.

**Why:** Unlike Python/Java where objects are always passed by reference-handle, C++ copies by default. Copying a big `std::string`/`std::vector` is real work; references avoid it.

**Example:**
```cpp
void f(std::string s);          // by value  → full copy; caller untouched
void f(std::string& s);         // by ref    → alias; can modify caller's object
void f(const std::string& s);   // const ref → alias; read-only, no copy  ← default for reads
```

**Rule of thumb:**
| Situation                         | Pass as     |
|-----------------------------------|-------------|
| read a big object                 | `const T&`  |
| modify the caller's object        | `T&`        |
| need your own copy to keep/store  | `T` (value) |
| small scalar (int/double/ptr/enum)| `T` (value) |

**Used in:** `addColumn(const std::string& name, DataType type)` — `name` by const-ref (big), `type` by value (small enum).

**Gotcha:** Returning a reference to a **local** variable = dangling reference (the local dies at return). References don't extend lifetime here.
