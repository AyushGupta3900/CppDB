# Tech-debt log

Deliberate shortcuts, why they were taken, and how to pay them back.

| # | What | Why deferred | How to fix | Where | Status |
|---|------|--------------|------------|-------|--------|
| 1 | `Database::getTable` returns a raw `Table*` that the next `createTable`/`dropTable` invalidates (HashMap rehash moves tables). | Stage 1 has no smart pointers yet. | Store `SharedPtr<Table>` in the map and return a copy. | `include/core/Database.hpp` | **Fixed in Stage 3** (tables held by `SharedPtr`, lookups return owning handles). |
| 2 | `Row` values are stored as `std::string` even for INT columns (parse on every `getInt`). | Keeps Stage 1 simple; matches the string-based query layer. | A `std::variant<int64_t, std::string>` cell type with typed accessors. | `include/core/Row.hpp` | Open |
