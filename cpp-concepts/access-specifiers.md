# Access Specifiers (`public` / `private` / `protected`)

**What:** Keywords controlling who can touch a member. `public` = anyone. `private` = only this class's own methods (and `friend`s). `protected` = this class + derived classes.

**Why:** Encapsulation — hide the data so the class controls all changes and can guarantee its invariants. Outside code can only go through the public methods you vet.

**Example:**
```cpp
class Schema {
public:                    // the interface
    void addColumn(const std::string&, DataType);
private:                   // the implementation detail — untouchable from outside
    std::vector<Column> columns_;
};
```
`someSchema.columns_` from outside → compile error. Good.

**Used in:** every class — data in `private`, methods in `public`.

**Gotcha:** `friend` grants another function/class access to your privates (we'll use it for `operator<<` on `Row`). Use sparingly.
