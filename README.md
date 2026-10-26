# MiniDB: Relational Database Engine in C++20

A fast, modular, embedded relational database engine written from scratch in modern **C++20** with an embedded C ABI and interactive CLI REPL. Modeled after the classical architectures of **SQLite** and **PostgreSQL** (and CMU 15-445 Bustub).

---

## Architecture Overview

```
+------------------------------------------------------------------------+
|                     Client CLI REPL & C ABI API                        |
+------------------------------------------------------------------------+
                                   |
+------------------------------------------------------------------------+
|                             Query Engine                               |
|   SQL Lexer -> Recursive Descent Parser -> Volcano Iterators (Scan/Idx)|
+------------------------------------------------------------------------+
                                   |
+------------------------------------------------------------------------+
|                      Catalog & Record Manager                          |
|   System Catalog, Type System, Schemas, Slotted Row Serialization      |
+------------------------------------------------------------------------+
                                   |
+------------------------------------------------------------------------+
|                           Storage Engine                               |
|   - Disk-Backed B+ Tree Indexing (splits, merges, range scans)         |
|   - TableHeap (linked list of 4KB slotted pages)                       |
|   - BufferPoolManager (Clock-replacer frame table caching)             |
|   - DiskManager (4096-byte page pread/pwrite with fsync)               |
+------------------------------------------------------------------------+
                                   |
+------------------------------------------------------------------------+
|                       Durability & Recovery                            |
|   - Write-Ahead Logging (WAL) with monotonic LSN tracking              |
+------------------------------------------------------------------------+
```

---

## Features

1. **Storage Layer**:
   - Fixed 4KB pages aligned with hardware blocks.
   - Variable-length slotted page engine with tombstone reuse and automatic on-page compaction/defragmentation.
   - Second-chance Clock replacer buffer pool caching with reader/writer latches.
2. **Indexing (B+ Tree)**:
   - Disk-backed multi-level B+ Tree supporting point lookups, dynamic node splits, underflow redistribution/coalescing, and sequential range iterators (`Begin()`, `Begin(key)`, `End()`).
3. **Record Management & Catalog**:
   - Typed values (`INTEGER`, `BIGINT`, `FLOAT`, `DOUBLE`, `VARCHAR`, `BOOLEAN`) with null-bitmap tuple serialization.
   - System catalog managing table schemas, table heaps, and secondary indexes.
4. **SQL Parser & Query Optimizer**:
   - Handcrafted lexer and recursive descent parser supporting DDL (`CREATE TABLE`, `CREATE INDEX`) and DML (`INSERT`, `SELECT`, `UPDATE`, `DELETE`).
   - Rule-based index selection: automatically routes point queries (`WHERE id = ?`) to an $O(\log N)$ `IndexScanExecutor` when an index exists, falling back to `SeqScanExecutor`.
5. **Interactive REPL**:
   - Multi-line SQL execution, pretty ASCII table output, execution timer, and meta-commands (`.tables`, `.schema`, `.timer`, `.flush`, `.exit`).
6. **Embedded C/C++ API**:
   - `minidb::Database::Open("db.db")` in C++, or `minidb_open()` / `minidb_execute()` in C.
7. **Write-Ahead Log (WAL)**:
   - Append-only binary WAL logging (`BEGIN`, `COMMIT`, `INSERT`, `UPDATE`) with synchronous flush guarantees.

---

## Building and Testing

### Prerequisites
- CMake 3.20+
- Modern C++20 compliant compiler (`Clang++` or `G++`)

### Build Steps
```bash
mkdir -p build && cd build
cmake ..
cmake --build .
```

### Running the Test Suite
```bash
ctest --output-on-failure
```
Six test suites are included:
- `BufferPoolTest`
- `SlottedPageTest`
- `BPlusTreeTest`
- `CatalogTest`
- `SQLExecutionTest`
- `WALTest`

---

## Interactive CLI Shell

Run the CLI shell:
```bash
./build/minidb_cli my_database.db
```

Example session:
```sql
minidb> CREATE TABLE users (id INT, name VARCHAR(50), balance DOUBLE);
Table 'users' created successfully. (0.074 ms)

minidb> INSERT INTO users VALUES (1, 'Alice', 500.0), (2, 'Bob', 1200.5), (3, 'Charlie', 42.0);
Inserted 3 row(s). (0.020 ms)

minidb> .tables
  - users

minidb> .schema users
Table: users
  id               INTEGER
  name             VARCHAR
  balance          DOUBLE

minidb> SELECT * FROM users WHERE balance > 100.0;
+----+-------+-------------+
| id | name  | balance     |
+----+-------+-------------+
| 1  | Alice | 500.000000  |
| 2  | Bob   | 1200.500000 |
+----+-------+-------------+
[Sequential Scan] 2 row(s) returned. (0.011 ms)

minidb> CREATE INDEX idx_users_id ON users (id);
Index 'idx_users_id' created on column 'id' (3 keys indexed). (0.010 ms)

minidb> SELECT * FROM users WHERE id = 2;
+----+------+-------------+
| id | name | balance     |
+----+------+-------------+
| 2  | Bob  | 1200.500000 |
+----+------+-------------+
[Index Scan] 1 row(s) returned. (0.008 ms)

minidb> .exit
Closing database and syncing disk buffers... Bye!
```

---

## Embedding in C / C++

### Modern C++ API
```cpp
#include "api/minidb.hpp"

int main() {
    auto db = minidb::Database::Open("app.db");
    auto res = db->Execute("SELECT * FROM users WHERE id = 1;");
    for (const auto& row : res.rows) {
        std::cout << row[0] << " " << row[1] << "\n";
    }
    return 0;
}
```

### Pure C ABI
```c
#include "api/minidb.hpp"

int main() {
    minidb_t* db = minidb_open("app.db");
    char* result = minidb_execute(db, "SELECT * FROM users;");
    printf("%s\n", result);
    minidb_free_string(result);
    minidb_close(db);
    return 0;
}
```
