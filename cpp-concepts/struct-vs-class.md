# `struct` vs `class`

**What:** Same feature. Only language difference: `struct` members are **public** by default, `class` members are **private** by default.

**Why:** The real distinction is *convention*: `struct` = plain data bag with no invariant; `class` = has rules to protect, so it hides data behind methods.

**Example:**
```cpp
struct Column { std::string name; DataType type; };  // any combo valid → struct

class Schema {                                        // must enforce "no dup names" → class
public:
    void addColumn(const std::string&, DataType);
private:
    std::vector<Column> columns_;
};
```

**Used in:** `Column` (struct), `Schema`/`Row`/`Table`/`Database` (classes).

**Gotcha:** Don't choose based on member count — choose on whether there's an **invariant** to protect.
