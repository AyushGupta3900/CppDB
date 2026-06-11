# Compilation & Linking

**What:** C++ builds in two phases. Each `.cpp` (a *translation unit*) is compiled **independently** into a `.o` object file; then the **linker** stitches all `.o` files into one executable.

**Why:** Explains C++'s biggest beginner trap — the difference between a *compile* error (this file is malformed) and a *link* error (a function was declared but its body was never compiled in → "undefined reference").

**Example:**
```
Schema.cpp ─compile→ Schema.o ─┐
test.cpp   ─compile→ test.o   ─┴─link→ executable
```
A header `#include` only lets the compiler *type-check* a call; the linker later finds the real body. Missing body = `Undefined symbols ... Schema::addColumn`.

**Used in:** the whole project; the Makefile compiles `src/*.cpp` + a test, then links.

**Gotcha:** "Undefined reference / Undefined symbols" is a **linker** error, not a typo — it means a declared function has no definition (or you forgot to add the `.cpp` to the build).
