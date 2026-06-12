# Tech-debt log

Deliberate shortcuts, why they were taken, and how to pay them back.

| # | What | Why deferred | How to fix | Where | Status |
|---|------|--------------|------------|-------|--------|
| 1 | `Database::getTable` returned a raw `Table*` that the next `createTable`/`dropTable` invalidated (HashMap rehash moves tables). | Stage 1 had no smart pointers yet. | Store `SharedPtr<Table>` in the map and return a copy. | `include/core/Database.hpp` | **Fixed in Stage 3** (tables held by `SharedPtr`, lookups return owning handles). |
| 2 | `Row` values were stored as `std::string` even for INT columns (parse on every `getInt`). | Kept Stage 1 simple. | Typed cells. | `include/core/Row.hpp` | **Fixed**: cells are `Value = std::variant<int64_t, std::string>`; INT literals parse exactly once in `Row::set`. |
| 3 | `UniquePtr`/`SharedPtr` have no custom deleters, arrays, `weak_ptr`, or aliasing ctor; `makeShared` does two allocations. | The teaching goal is ownership + refcounting, not full std parity. | Deleter template param; single-allocation control block. | `include/memory/` | **Accepted (scope)** — nothing in this codebase needs those features; revisit only if a real use appears. |
| 4 | `PoolAllocator` is not wired into `Table` row storage. | Rows live *by value inside B-tree nodes* — that is the clustered-storage design, so there is no per-row heap allocation for a pool to absorb. The pool's win over `new`/`delete` is measured in `make bench` (~24×). | If rows ever move out-of-node (e.g. variable-length pages), allocate them from the pool. | `include/storage/PoolAllocator.hpp` | **Accepted (design)** — documented in README. |
| 5 | WHERE supported exactly one `col op literal` condition; no `UPDATE` statement. | Kept the parser a clean single-pass teaching example. | Condition list + UPDATE. | `src/query/` | **Fixed**: `WHERE a AND b AND ...` (id clauses still use the index) and `UPDATE ... SET ... [WHERE ...]` with all-or-nothing validation. `OR` remains unsupported. |
| 6 | String literals had no escape sequences. | Rarely needed for the demo workload. | Doubled-quote handling in the lexer. | `src/query/lexer.cpp` | **Fixed**: SQL-style `''` / `""` escapes. |
| 7 | No persistence — the database is memory-only by design. | The project's scope is in-memory engineering (STARTER.md lists disk pages as a post-project bonus). | Write-ahead log + page flush in `storage/PageManager`. | — | Open (future work) |
