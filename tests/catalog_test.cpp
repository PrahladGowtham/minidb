#include "catalog/catalog.hpp"
#include <cassert>
#include <iostream>
#include <vector>

void TestTupleSerialization() {
    std::cout << "[Test] Tuple Serialization & Deserialization..." << std::endl;

    std::vector<minidb::Column> cols = {
        minidb::Column("id", minidb::TypeId::INTEGER),
        minidb::Column("name", minidb::TypeId::VARCHAR, true, 32),
        minidb::Column("balance", minidb::TypeId::DOUBLE),
        minidb::Column("is_active", minidb::TypeId::BOOLEAN),
        minidb::Column("notes", minidb::TypeId::VARCHAR, true, 64)
    };
    minidb::Schema schema(cols);

    std::vector<minidb::Value> values = {
        minidb::Value(static_cast<int32_t>(101)),
        minidb::Value(std::string("Alice Developer")),
        minidb::Value(static_cast<double>(1234.56)),
        minidb::Value(true),
        minidb::Value(minidb::TypeId::VARCHAR) // NULL
    };

    minidb::Tuple tuple(values, &schema);

    assert(tuple.GetValue(&schema, 0).GetAsInteger() == 101);
    assert(tuple.GetValue(&schema, 1).GetAsVarchar() == "Alice Developer");
    assert(tuple.GetValue(&schema, 2).GetAsDouble() == 1234.56);
    assert(tuple.GetValue(&schema, 3).GetAsBoolean() == true);
    assert(tuple.GetValue(&schema, 4).IsNull());

    std::cout << "  Tuple string: " << tuple.ToString(&schema) << std::endl;
    std::cout << "  Passed!" << std::endl;
}

void TestTableHeapAndCatalog() {
    std::cout << "[Test] TableHeap & Catalog Multi-Page Ingestion..." << std::endl;
    const std::string db_name = "test_cat.db";
    std::remove(db_name.c_str());

    auto disk_manager = std::make_unique<minidb::DiskManager>(db_name);
    minidb::BufferPoolManager bpm(20, disk_manager.get());
    minidb::Catalog catalog(&bpm);

    std::vector<minidb::Column> cols = {
        minidb::Column("user_id", minidb::TypeId::BIGINT),
        minidb::Column("username", minidb::TypeId::VARCHAR, false, 64)
    };
    minidb::Schema schema(cols);

    minidb::TableInfo* table_info = catalog.CreateTable("users", schema);
    assert(table_info != nullptr);
    assert(catalog.GetTable("users") == table_info);

    // Insert 500 rows to ensure multi-page table heap chain
    for (int64_t i = 1; i <= 500; ++i) {
        std::vector<minidb::Value> vals = {
            minidb::Value(i),
            minidb::Value("User_" + std::to_string(i))
        };
        minidb::Tuple t(vals, &schema);
        minidb::RID rid;
        assert(table_info->table_heap->InsertTuple(t, &rid));
        assert(rid.IsValid());
    }

    // Iterate through TableIterator and verify count & values
    int64_t expected_id = 1;
    int count = 0;
    for (auto it = table_info->table_heap->Begin(&schema); it != table_info->table_heap->End(); ++it) {
        minidb::Value v_id = it->GetValue(&schema, 0);
        minidb::Value v_name = it->GetValue(&schema, 1);

        assert(v_id.GetAsBigInt() == expected_id);
        assert(v_name.GetAsVarchar() == ("User_" + std::to_string(expected_id)));

        expected_id++;
        count++;
    }
    assert(count == 500);

    // Create an index on column 0 (user_id)
    minidb::IndexInfo* idx_info = catalog.CreateIndex("idx_users_id", "users", {0});
    assert(idx_info != nullptr);
    assert(catalog.GetIndex("idx_users_id") == idx_info);

    std::remove(db_name.c_str());
    std::cout << "  Passed!" << std::endl;
}

int main() {
    std::cout << "=== Running Catalog & Table Heap Unit Tests ===" << std::endl;
    TestTupleSerialization();
    TestTableHeapAndCatalog();
    std::cout << "All Catalog tests passed successfully!\n" << std::endl;
    return 0;
}
