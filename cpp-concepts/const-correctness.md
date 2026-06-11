# const-correctness

**What:** Using `const` to make read-only promises the compiler enforces — on parameters, on member functions, and on variables.

**Why:** `const` is a machine-checked contract. A `const` member function can be called on a `const` object; without it, passing objects as `const T&` (the cheap default) makes their methods uncallable.

**Example:**
```cpp
class Schema {
public:
    bool hasColumn(const std::string& name) const;  // (a) const param (b) const method
    size_t columnCount() const;                      // reads only → const
    void addColumn(const std::string&, DataType);    // mutates → NOT const
};

void print(const Schema& s) {     // cheap, read-only handle
    s.columnCount();              // OK — columnCount is const
    // s.addColumn(...);          // ERROR — can't mutate a const Schema
}
```

**Used in:** every read-only method in every class.

**Gotcha:** Mark methods `const` *as you write them*. Retrofitting later cascades (one const method calling a non-const one fails to compile). Inside a `const` method, all members are treated as `const`. Escape hatch for caches/locks: declare that member `mutable`.
