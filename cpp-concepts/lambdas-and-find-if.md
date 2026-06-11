# Lambdas & `std::find_if`

**What:** A *lambda* is an inline anonymous function. `std::find_if` is an STL algorithm that returns an iterator to the first element matching a predicate (often a lambda).

**Why:** Lets you express "find the column named X" as a one-liner instead of a hand-written loop, and is the gateway to STL algorithms generally.

**Example:**
```cpp
#include <algorithm>
auto it = std::find_if(columns_.begin(), columns_.end(),
                       [&](const Column& c){ return c.name == name; });
//                      ^^^ capture     ^^^^^^^^^^ params      ^^^^^ body
bool found = (it != columns_.end());     // end() means "not found"
```
Lambda capture: `[&]` = capture surrounding variables by reference, `[=]` by value, `[name]` just `name`.

**Used in:** searching `Schema` columns (alternative to a manual loop); later in sorting/filtering.

**Gotcha:** `find_if` returns `end()` on no match — always compare against `end()` before dereferencing `*it`. Capturing by reference (`[&]`) a variable that dies before the lambda runs = dangling (matters for stored/async lambdas, not immediate calls like this).
