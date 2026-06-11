# Naming Conventions (this project)

**What:** Consistent names so you can tell a member from a local at a glance.

**Why:** C++ has no enforced style. Picking one and sticking to it removes a whole class of "is this `this->x` or a parameter?" confusion.

**Convention used here:**
| Thing            | Style            | Example            |
|------------------|------------------|--------------------|
| private member   | trailing `_`     | `columns_`, `ptr_` |
| class / struct   | PascalCase       | `Schema`, `Column` |
| function / var    | camelCase        | `addColumn`, `name`|
| enum class value | UPPER or Pascal  | `DataType::INT`    |
| constant         | `kPascal` or UPPER | `kMaxKeys`       |

**Used in:** everywhere.

**Gotcha:** Don't prefix with `_` at the *start* of a name (`_name`) — leading underscores are reserved for the implementation in many contexts. Trailing underscore (`name_`) is safe.
