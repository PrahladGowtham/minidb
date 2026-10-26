#include "recovery/log_manager.hpp"
#include <cassert>
#include <iostream>
#include <cstdio>

void TestWALAppendAndRead() {
    std::cout << "[Test] WAL Append, Flush, and Replay Verification..." << std::endl;
    const std::string wal_file = "test_wal.log";
    std::remove(wal_file.c_str());

    // 1. Write WAL records
    {
        minidb::LogManager log_mgr(wal_file);

        // Transaction 1 BEGIN
        minidb::LogRecord r1(1, minidb::INVALID_LSN, minidb::LogRecordType::BEGIN);
        minidb::lsn_t lsn1 = log_mgr.AppendRecord(&r1);
        assert(lsn1 == 0);

        // Transaction 1 INSERT
        std::vector<minidb::Column> cols = {minidb::Column("id", minidb::TypeId::INTEGER)};
        minidb::Schema schema(cols);
        minidb::Tuple t({minidb::Value(42)}, &schema);
        minidb::RID rid(1, 0);

        minidb::LogRecord r2(1, lsn1, minidb::LogRecordType::INSERT, rid, t);
        minidb::lsn_t lsn2 = log_mgr.AppendRecord(&r2);
        assert(lsn2 == 1);

        // Transaction 1 COMMIT
        minidb::LogRecord r3(1, lsn2, minidb::LogRecordType::COMMIT);
        minidb::lsn_t lsn3 = log_mgr.AppendRecord(&r3);
        assert(lsn3 == 2);

        log_mgr.Flush();
        assert(log_mgr.GetFlushedLSN() == 2);
    }

    // 2. Re-open WAL and replay records
    {
        minidb::LogManager log_mgr(wal_file);
        auto records = log_mgr.ReadAllRecords();
        assert(records.size() == 3);

        assert(records[0].GetLSN() == 0);
        assert(records[0].GetType() == minidb::LogRecordType::BEGIN);
        assert(records[0].GetTxnId() == 1);

        assert(records[1].GetLSN() == 1);
        assert(records[1].GetType() == minidb::LogRecordType::INSERT);
        assert(records[1].GetTxnId() == 1);
        assert(records[1].GetPrevLSN() == 0);
        assert(records[1].GetRID() == minidb::RID(1, 0));

        std::vector<minidb::Column> cols = {minidb::Column("id", minidb::TypeId::INTEGER)};
        minidb::Schema schema(cols);
        assert(records[1].GetTuple().GetValue(&schema, 0).GetAsInteger() == 42);

        assert(records[2].GetLSN() == 2);
        assert(records[2].GetType() == minidb::LogRecordType::COMMIT);
        assert(records[2].GetTxnId() == 1);
        assert(records[2].GetPrevLSN() == 1);
    }

    std::remove(wal_file.c_str());
    std::cout << "  Passed!" << std::endl;
}

int main() {
    std::cout << "=== Running WAL Recovery Unit Tests ===" << std::endl;
    TestWALAppendAndRead();
    std::cout << "All WAL tests passed successfully!\n" << std::endl;
    return 0;
}
