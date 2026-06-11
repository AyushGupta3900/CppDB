# C++ Concepts Catalogue

A **granular, growing glossary** — one file per atomic C++ concept, added the moment it first shows up in the project. Use this to look up a single idea fast.

> Difference from `../concepts/`: that folder has **lessons** (several concepts woven into a build task). This folder has **atoms** (one concept, defined and exemplified). Lessons link here.

Each file follows the same template: **What · Why · Example · Used in · Gotcha**.

## Index (grows over time)

### Compilation & files
- [compilation-and-linking](compilation-and-linking.md) — translation units, compile → link
- [headers-and-pragma-once](headers-and-pragma-once.md) — `.hpp`/`.cpp` split, include guards

### Types & declarations
- [struct-vs-class](struct-vs-class.md)
- [access-specifiers](access-specifiers.md) — public/private
- [enum-class](enum-class.md) — scoped enums
- [static-cast](static-cast.md)
- [naming-conventions](naming-conventions.md) — `member_`, etc.

### Values, references, const
- [passing-value-reference-constref](passing-value-reference-constref.md)
- [const-correctness](const-correctness.md) — const params + const methods

### STL building blocks
- [std-string](std-string.md)
- [std-vector](std-vector.md)
- [range-based-for](range-based-for.md)
- [lambdas-and-find-if](lambdas-and-find-if.md)

### Tooling
- [assert](assert.md)

---
**Concept count so far: 14.** Added through: Lesson 1 (Schema).
