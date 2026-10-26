#include "execution/execution_engine.hpp"
#include <cassert>
#include <iostream>

void TestFullSQLExecution() {
    std::cout << "[Test] Full SQL Query Lifecycle (DDL, DML, Indexes, Optimizer)..." << std::endl;
    const std::string db_name = "test_sql.db";
    std::remove(db_name.c_str());

    auto disk_manager = std::make_unique<minidb::DiskManager>(db_name);
    minidb::BufferPoolManager bpm(50, disk_manager.get());
    minidb::Catalog catalog(&bpm);
    minidb::ExecutionEngine engine(&catalog, &bpm);

    // 1. CREATE TABLE
    auto res_ddl = engine.Execute("CREATE TABLE accounts (id INT, name VARCHAR(50), balance DOUBLE);");
    assert(res_ddl.success);
    std::cout << "  " << res_ddl.message << std::endl;

    // 2. INSERT VALUES
    auto res_ins = engine.Execute(
        "INSERT INTO accounts VALUES (1, 'Alice', 500.0), (2, 'Bob', 1200.5), (3, 'Charlie', 42.0);"
    );
    assert(res_ins.success);
    assert(res_ins.rows_affected == 3);
    std::cout << "  " << res_ins.message << std::endl;

    // 3. SELECT * FROM accounts (SeqScan)
    auto res_sel1 = engine.Execute("SELECT * FROM accounts;");
    assert(res_sel1.success);
    assert(res_sel1.rows.size() == 3);
    assert(res_sel1.message.find("[Sequential Scan]") != std::string::npos);
    std::cout << "  " << res_sel1.message << std::endl;

    // 4. SELECT with WHERE predicate
    auto res_sel2 = engine.Execute("SELECT name, balance FROM accounts WHERE balance > 100.0;");
    assert(res_sel2.success);
    assert(res_sel2.rows.size() == 2);
    assert(res_sel2.column_names.size() == 2);
    assert(res_sel2.rows[0][0] == "Alice");
    assert(res_sel2.rows[1][0] == "Bob");

    // 5. CREATE INDEX ON id
    auto res_idx = engine.Execute("CREATE INDEX idx_acc_id ON accounts (id);");
    assert(res_idx.success);
    std::cout << "  " << res_idx.message << std::endl;

    // 6. SELECT WHERE id = 2 (Should use Index Scan!)
    auto res_idx_scan = engine.Execute("SELECT * FROM accounts WHERE id = 2;");
    assert(res_idx_scan.success);
    assert(res_idx_scan.rows.size() == 1);
    assert(res_idx_scan.rows[0][1] == "Bob");
    assert(res_idx_scan.message.find("[Index Scan]") != std::string::npos);
    std::cout << "  " << res_idx_scan.message << " (Row: " << res_idx_scan.rows[0][0] << ", "
              << res_idx_scan.rows[0][1] << ", " << res_idx_scan.rows[0][2] << ")" << std::endl;

    // 7. UPDATE
    auto res_upd = engine.Execute("UPDATE accounts SET balance = 9999.0 WHERE id = 1;");
    assert(res_upd.success);
    assert(res_upd.rows_affected == 1);

    auto res_check_upd = engine.Execute("SELECT balance FROM accounts WHERE id = 1;");
    assert(res_check_upd.rows[0][0] == "9999.000000");

    // 8. DELETE
    auto res_del = engine.Execute("DELETE FROM accounts WHERE id = 3;");
    assert(res_del.success);
    assert(res_del.rows_affected == 1);

    auto res_count = engine.Execute("SELECT * FROM accounts;");
    assert(res_count.rows.size() == 2);

    std::remove(db_name.c_str());
    std::cout << "  Passed!" << std::endl;
}

int main() {
    std::cout << "=== Running SQL Query Execution Engine Unit Tests ===" << std::endl;
    TestFullSQLExecution();
    std::cout << "All SQL Query Execution tests passed successfully!\n" << std::endl;
    return 0;
}
