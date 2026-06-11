# Headers & `#pragma once`

**What:** Declarations go in a header (`.hpp`); definitions (bodies) go in a source (`.cpp`). `#pragma once` at the top of every header stops it being included twice in one translation unit.

**Why:** `#include` literally pastes the header's text. Include the same header twice (directly + transitively) and the class gets defined twice → "redefinition" error. `#pragma once` prevents that.

**Example:**
```cpp
// Schema.hpp
#pragma once
#include <string>
class Schema { public: bool hasColumn(const std::string&) const; };  // declaration

// Schema.cpp
#include "core/Schema.hpp"
bool Schema::hasColumn(const std::string& n) const { /* body */ }     // definition
```

**Used in:** every `.hpp` in `include/`.

**Gotcha:** A header should `#include` what *it* names (here `<string>`), not rely on whoever includes it having done so. Don't put `using namespace std;` in a header — it leaks into every file.
