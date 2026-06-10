# CppDB — Thread-Safe In-Memory Database in C++

A from-scratch C++ project that teaches OOP, Memory Management, Data Structures, and Concurrency through building a mini Redis-like database.

---

## What You Will Learn

| Stage | Concepts Covered |
|-------|-----------------|
| Stage 1 | OOP, Templates, Operator Overloading, Rule of Five |
| Stage 2 | Memory Management, Pool Allocator, RAII, Smart Pointers |
| Stage 3 | Threads, Mutex, RWLock, Condition Variables, Atomics |
| Stage 4 | B-Tree Index, Pointer Structures, Node Splitting |
| Stage 5 | LRU Cache, Doubly Linked List + HashMap combo |
| Stage 6 | Query Parsing, Tokenizer, Basic Compiler Frontend |
| Stage 7 | TCP Server, epoll, Non-blocking I/O |

---

## Project Structure

```
CppDB/
├── include/
│   ├── core/
│   │   ├── Database.hpp        # Top-level DB class
│   │   ├── Table.hpp           # Holds rows, manages schema
│   │   ├── Row.hpp             # Single record
│   │   └── Schema.hpp          # Column definitions + types
│   ├── storage/
│   │   ├── MemoryAllocator.hpp # Pool allocator for Row objects
│   │   └── PageManager.hpp     # Memory page abstraction
│   ├── structures/
│   │   ├── HashMap.hpp         # Custom open-addressing hashmap
│   │   ├── BTree.hpp           # B-Tree for primary key index
│   │   └── LRUCache.hpp        # LRU cache (list + hashmap)
│   ├── concurrency/
│   │   ├── ThreadPool.hpp      # Fixed-size worker thread pool
│   │   ├── RWLock.hpp          # Readers-writer lock
│   │   └── LockFreeQueue.hpp   # MPSC lock-free task queue
│   ├── query/
│   │   ├── QueryParser.hpp     # Tokenizer + AST builder
│   │   └── QueryExecutor.hpp   # Executes parsed queries
│   └── network/
│       └── TCPServer.hpp       # TCP interface via epoll
├── src/                        # .cpp implementations
├── tests/                      # Unit tests per module
├── CMakeLists.txt
└── main.cpp
```

---

## Stage 1 — Core OOP & Data Structures

**Goal:** Model the database entities and store them in a custom HashMap.

### Classes to Build

```cpp
// Schema — defines columns
class Schema {
public:
    void addColumn(const std::string& name, DataType type);
    bool validate(const Row& row) const;
private:
    std::vector<Column> columns_;
};

// Row — a single record
class Row {
public:
    Row(const Schema& schema);
    Row(const Row& other);            // copy constructor
    Row(Row&& other) noexcept;        // move constructor
    Row& operator=(const Row& other); // copy assignment
    Row& operator=(Row&& other);      // move assignment
    ~Row();

    void set(const std::string& col, const std::string& val);
    std::string get(const std::string& col) const;
    friend std::ostream& operator<<(std::ostream& os, const Row& row);
private:
    std::unordered_map<std::string, std::string> data_;
    const Schema& schema_;
};

// Table — holds rows, owns schema
class Table {
public:
    explicit Table(Schema schema);
    void insert(Row row);
    Row* findById(int id);
    void deleteById(int id);
private:
    Schema schema_;
    HashMap<int, Row> rows_;   // your custom HashMap
};

// Database — top-level, owns tables
class Database {
public:
    void createTable(const std::string& name, Schema schema);
    Table* getTable(const std::string& name);
    void dropTable(const std::string& name);
private:
    HashMap<std::string, Table> tables_;
};
```

### Key C++ Concepts Practiced
- **Rule of Five** on `Row` (destructor, copy ctor, move ctor, copy assign, move assign)
- **Templates** on `HashMap<K, V>`
- **Operator overloading** (`<<` for Row, `[]` for HashMap)
- **Composition** — Database owns Tables, Tables own Rows

---

## Stage 2 — Memory Management

**Goal:** All Row allocations go through a custom pool allocator. No raw `new`/`delete` outside allocator code.

### Classes to Build

```cpp
// Pool allocator — fixed-size block allocator
template<typename T>
class PoolAllocator {
public:
    explicit PoolAllocator(size_t poolSize);
    ~PoolAllocator();

    T* allocate();
    void deallocate(T* ptr);
private:
    void* pool_;           // raw memory block
    std::stack<void*> freeList_;
    size_t blockSize_;
};

// Your own UniquePtr
template<typename T>
class UniquePtr {
public:
    explicit UniquePtr(T* ptr = nullptr);
    ~UniquePtr();
    UniquePtr(const UniquePtr&) = delete;            // no copy
    UniquePtr(UniquePtr&& other) noexcept;           // move only
    UniquePtr& operator=(UniquePtr&& other) noexcept;
    T* operator->() const;
    T& operator*() const;
    T* get() const;
    T* release();
    void reset(T* ptr = nullptr);
private:
    T* ptr_;
};

// Your own SharedPtr with ref counting
template<typename T>
class SharedPtr {
public:
    explicit SharedPtr(T* ptr = nullptr);
    SharedPtr(const SharedPtr& other);     // increments count
    SharedPtr(SharedPtr&& other) noexcept; // transfers ownership
    ~SharedPtr();                          // decrements count, deletes if 0
    int useCount() const;
private:
    T* ptr_;
    int* refCount_;
};
```

### Key C++ Concepts Practiced
- **Placement new** inside pool allocator
- **RAII** — resources tied to object lifetime
- **Move semantics** — UniquePtr is move-only
- **Reference counting** — SharedPtr manual implementation
- **`= delete`** to enforce ownership rules

---

## Stage 3 — Concurrency

**Goal:** Multiple clients query the DB simultaneously. Reads run in parallel, writes are exclusive.

### Classes to Build

```cpp
// Readers-Writer Lock
class RWLock {
public:
    void readLock();
    void readUnlock();
    void writeLock();
    void writeUnlock();
private:
    std::mutex mutex_;
    std::condition_variable cv_;
    int readers_ = 0;
    bool writing_ = false;
};

// Lock-free MPSC queue using atomics
template<typename T>
class LockFreeQueue {
public:
    void push(T item);
    bool pop(T& item);
private:
    struct Node {
        T data;
        std::atomic<Node*> next;
    };
    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;
};

// Thread pool
class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads);
    ~ThreadPool();
    void submit(std::function<void()> task);
private:
    std::vector<std::thread> workers_;
    LockFreeQueue<std::function<void()>> taskQueue_;
    std::atomic<bool> stop_;
    std::mutex mutex_;
    std::condition_variable cv_;
};
```

### Apply to Table
```cpp
class Table {
    // Each table gets its own RWLock
    // SELECT  → readLock / readUnlock
    // INSERT/UPDATE/DELETE → writeLock / writeUnlock
    mutable RWLock lock_;
};
```

### Key C++ Concepts Practiced
- `std::thread`, `std::mutex`, `std::condition_variable`
- `std::atomic<T>` and memory ordering (`memory_order_acquire`, `memory_order_release`)
- Lock-free programming with CAS (`compare_exchange_weak`)
- Deadlock prevention strategies
- RAII lock guards

---

## Stage 4 — B-Tree Index

**Goal:** Primary key lookups go from O(n) to O(log n) via a B-Tree index.

```cpp
template<typename K, typename V>
class BTree {
public:
    void insert(K key, V value);
    V* search(K key);
    void remove(K key);
private:
    struct Node {
        std::vector<K> keys;
        std::vector<V> values;
        std::vector<UniquePtr<Node>> children;
        bool isLeaf;
    };
    void splitChild(Node* parent, int index);
    UniquePtr<Node> root_;
    int order_;   // max keys per node
};
```

### Key C++ Concepts Practiced
- Recursive pointer structures
- Node splitting and tree rebalancing
- Using your own `UniquePtr` in a real structure

---

## Stage 5 — LRU Cache

**Goal:** Cache the last N accessed rows to avoid repeated lookups.

```cpp
template<typename K, typename V>
class LRUCache {
public:
    explicit LRUCache(size_t capacity);
    void put(K key, V value);
    V* get(K key);          // returns nullptr on miss
private:
    size_t capacity_;
    std::list<std::pair<K, V>> list_;            // front = most recent
    HashMap<K, typename std::list<std::pair<K,V>>::iterator> map_;
};
```

### Key C++ Concepts Practiced
- Iterator invalidation rules
- Combining two data structures for O(1) operations
- Template + STL interop

---

## Stage 6 — Query Parser

**Goal:** Accept string queries and execute them against the DB.

### Supported syntax
```sql
INSERT INTO users (id, name, age) VALUES (1, "Alice", 30)
SELECT * FROM users WHERE id = 1
DELETE FROM users WHERE id = 1
CREATE TABLE users (id INT, name TEXT, age INT)
```

### Classes to Build
```cpp
enum class TokenType { KEYWORD, IDENTIFIER, NUMBER, STRING, OPERATOR, PUNCTUATION };

struct Token {
    TokenType type;
    std::string value;
};

class Lexer {
public:
    explicit Lexer(const std::string& input);
    std::vector<Token> tokenize();
};

class QueryParser {
public:
    explicit QueryParser(std::vector<Token> tokens);
    QueryAST parse();   // returns an AST node
};

class QueryExecutor {
public:
    explicit QueryExecutor(Database& db);
    std::string execute(const std::string& query);
};
```

---

## Stage 7 — TCP Server

**Goal:** Connect to CppDB over a socket like a real database client.

```bash
$ telnet localhost 5432
> INSERT INTO users (id, name) VALUES (1, "Alice")
OK
> SELECT * FROM users WHERE id = 1
id=1, name=Alice
```

```cpp
class TCPServer {
public:
    explicit TCPServer(int port);
    void start();
private:
    void handleClient(int clientFd);
    int serverFd_;
    int epollFd_;
    QueryExecutor executor_;
};
```

### Key C++ Concepts Practiced
- `epoll` for non-blocking I/O
- File descriptors, socket programming
- Framing (length-prefixed messages)

---

## Build & Run

```bash
# Requirements: g++ 17+, CMake 3.15+
git clone https://github.com/you/CppDB
cd CppDB
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j4
./CppDB
```

---

## Recommended Order of Study

1. Finish Stage 1 completely before moving on — OOP fundamentals must be solid
2. Write tests for every class as you build it (`/tests/`)
3. Read: *Effective Modern C++* by Scott Meyers alongside Stage 2
4. Read: *C++ Concurrency in Action* by Anthony Williams alongside Stage 3
5. After Stage 7 — add persistence (write pages to disk) as a bonus challenge

---

## Senior C++ Skills This Project Proves

- You understand memory — not just how to use it, but how to manage it
- You can write thread-safe code without races or deadlocks
- You know data structures at the implementation level, not just the API level
- You've built a real system end-to-end, not just solved LeetCode problems