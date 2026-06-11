# Tech-debt log

Deliberate shortcuts, why they were taken, and how to pay them back.

| # | What | Why deferred | How to fix | Where | Status |
|---|------|--------------|------------|-------|--------|
| 1 | `Database::getTable` returns a raw `Table*` that the next `createTable`/`dropTable` invalidates (HashMap rehash moves tables). | Stage 1 has no smart pointers yet. | Store `SharedPtr<Table>` in the map and return a copy. | `include/core/Database.hpp` | **Fixed in Stage 3** (tables held by `SharedPtr`, lookups return owning handles). |
| 2 | `Row` values are stored as `std::string` even for INT columns (parse on every `getInt`). | Keeps Stage 1 simple; matches the string-based query layer. | A `std::variant<int64_t, std::string>` cell type with typed accessors. | `include/core/Row.hpp` | Open |
| 3 | `UniquePtr`/`SharedPtr` have no custom deleters, arrays, `weak_ptr`, or aliasing ctor; `makeShared` does two allocations (object + control block). | The teaching goal is ownership + refcounting, not full std parity. | Deleter template param; single-allocation control block like `std::make_shared`. | `include/memory/` | Open |
| 4 | `PoolAllocator` is not wired into `Table` row storage (HashMap stores `Row` by value). | Allocator-aware containers are a large detour; the pool's win is shown in `tests/bench.cpp`. | Give HashMap an allocator template param, or store `Row*` from the pool. | `include/storage/PoolAllocator.hpp` | Open |
