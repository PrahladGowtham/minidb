#pragma once

#include "catalog/schema.hpp"
#include "storage/table/table_heap.hpp"
#include "index/b_plus_tree.hpp"

#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>

namespace minidb {

using table_oid_t = uint32_t;
using index_oid_t = uint32_t;

struct Int64Comparator {
    int operator()(int64_t a, int64_t b) const {
        if (a < b) return -1;
        if (a > b) return 1;
        return 0;
    }
};

using IndexTree = BPlusTree<int64_t, RID, Int64Comparator>;

struct TableInfo {
    table_oid_t oid;
    std::string name;
    Schema schema;
    std::unique_ptr<TableHeap> table_heap;
};

struct IndexInfo {
    index_oid_t oid;
    std::string name;
    std::string table_name;
    std::vector<uint32_t> key_attrs;
    std::unique_ptr<IndexTree> index;
};

/**
 * Catalog manages table definitions, schemas, table heaps, and B+ Tree indexes.
 */
class Catalog {
public:
    explicit Catalog(BufferPoolManager* bpm) : bpm_(bpm) {}

    TableInfo* CreateTable(const std::string& name, const Schema& schema);
    [[nodiscard]] TableInfo* GetTable(const std::string& name) const;
    [[nodiscard]] TableInfo* GetTable(table_oid_t oid) const;

    IndexInfo* CreateIndex(const std::string& index_name, const std::string& table_name,
                           const std::vector<uint32_t>& key_attrs);
    [[nodiscard]] IndexInfo* GetIndex(const std::string& index_name) const;
    [[nodiscard]] std::vector<IndexInfo*> GetTableIndexes(const std::string& table_name) const;

    [[nodiscard]] std::vector<std::string> GetTableNames() const;

private:
    BufferPoolManager* bpm_;
    std::unordered_map<std::string, table_oid_t> table_names_;
    std::unordered_map<table_oid_t, std::unique_ptr<TableInfo>> tables_;
    table_oid_t next_table_oid_{1};

    std::unordered_map<std::string, index_oid_t> index_names_;
    std::unordered_map<index_oid_t, std::unique_ptr<IndexInfo>> indexes_;
    index_oid_t next_index_oid_{1};

    mutable std::mutex catalog_mutex_;
};

} // namespace minidb
