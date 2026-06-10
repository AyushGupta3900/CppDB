# Syntax 01 — Classes, Structs, Enums, const

Quick reference. Concept/why → `../concepts/01-classes-structs-enums.md`.

## Header guard
```cpp
#pragma once          // top of EVERY .hpp
```

## Scoped enum
```cpp
enum class DataType { INT, TEXT, BOOL };
DataType t = DataType::INT;
int n = static_cast<int>(t);              // explicit only
```

## Struct (public by default — data bag)
```cpp
struct Column {
    std::string name;
    DataType    type;
};
Column c{"age", DataType::INT};           // aggregate init
Column d = {.name = "age", .type = DataType::INT};  // designated init (C++20)
```

## Class (private by default — has invariants)
```cpp
class Schema {
public:
    Schema() = default;                    // explicitly default the ctor
    void addColumn(const std::string& name, DataType type);
    bool hasColumn(const std::string& name) const;   // const = read-only method
    size_t columnCount() const;
private:
    std::vector<Column> columns_;
};
```

## Member function definition (in .cpp)
```cpp
#include "core/Schema.hpp"

void Schema::addColumn(const std::string& name, DataType type) {
    columns_.push_back(Column{name, type});
}

bool Schema::hasColumn(const std::string& name) const {
    for (const Column& c : columns_)       // range-for by const ref
        if (c.name == name) return true;
    return false;
}

size_t Schema::columnCount() const {
    return columns_.size();
}
```

## Parameter passing cheat-sheet
| Want to…                       | Signature              |
|--------------------------------|------------------------|
| read a big object, no copy     | `const T&`             |
| modify caller's object         | `T&`                   |
| take your own copy             | `T` (by value)         |
| pass a small scalar            | `T` (int, double, ptr) |

## const placements
```cpp
void f(const std::string& s);     // param: won't modify s
int  size() const;                // method: won't modify *this
const Column& at(size_t i) const; // returns read-only reference
```

## Range-based for
```cpp
for (const Column& c : columns_) { /* read */ }
for (Column& c : columns_)       { /* modify */ }
for (auto& c : columns_)         { /* let compiler deduce type */ }
```

## switch over enum class (for the stretch goal)
```cpp
std::string typeName(DataType t) {
    switch (t) {
        case DataType::INT:  return "INT";
        case DataType::TEXT: return "TEXT";
        case DataType::BOOL: return "BOOL";
    }
    return "?";
}
```

## Find in a vector (std::find_if — alternative to a manual loop)
```cpp
#include <algorithm>
auto it = std::find_if(columns_.begin(), columns_.end(),
                       [&](const Column& c){ return c.name == name; });
bool found = (it != columns_.end());
```

## Minimal test scaffold (tests/schema.cpp)
```cpp
#include "core/Schema.hpp"
#include <cassert>
#include <iostream>

int main() {
    Schema s;
    s.addColumn("id", DataType::INT);
    assert(s.hasColumn("id"));
    assert(!s.hasColumn("nope"));
    assert(s.columnCount() == 1);
    std::cout << "all tests passed\n";
    return 0;
}
```
> `assert(cond)` aborts with a message if `cond` is false. Compiled-in because we don't pass `-DNDEBUG`. Great for quick tests.
