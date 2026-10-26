#include "catalog/catalog.hpp"

namespace minidb {

TableInfo* Catalog::CreateTable(const std::string& name, const Schema& schema) {
    std::lock_guard<std::mutex> lock(catalog_mutex_);
    if (table_names_.find(name) != table_names_.end()) {
        throw DBException("Table already exists: " + name);
    }

    table_oid_t oid = next_table_oid_++;
    auto heap = std::make_unique<TableHeap>(bpm_);
    auto info = std::make_unique<TableInfo>(TableInfo{
        .oid = oid,
        .name = name,
        .schema = schema,
        .table_heap = std::move(heap)
    });

    TableInfo* ptr = info.get();
    tables_[oid] = std::move(info);
    table_names_[name] = oid;
    return ptr;
}

TableInfo* Catalog::GetTable(const std::string& name) const {
    std::lock_guard<std::mutex> lock(catalog_mutex_);
    auto it = table_names_.find(name);
    if (it == table_names_.end()) {
        return nullptr;
    }
    return tables_.at(it->second).get();
}

TableInfo* Catalog::GetTable(table_oid_t oid) const {
    std::lock_guard<std::mutex> lock(catalog_mutex_);
    auto it = tables_.find(oid);
    if (it == tables_.end()) {
        return nullptr;
    }
    return it->second.get();
}

IndexInfo* Catalog::CreateIndex(const std::string& index_name, const std::string& table_name,
                                const std::vector<uint32_t>& key_attrs) {
    std::lock_guard<std::mutex> lock(catalog_mutex_);
    if (index_names_.find(index_name) != index_names_.end()) {
        throw DBException("Index already exists: " + index_name);
    }

    index_oid_t oid = next_index_oid_++;
    auto bpt = std::make_unique<IndexTree>(index_name, bpm_, Int64Comparator());

    auto info = std::make_unique<IndexInfo>(IndexInfo{
        .oid = oid,
        .name = index_name,
        .table_name = table_name,
        .key_attrs = key_attrs,
        .index = std::move(bpt)
    });

    IndexInfo* ptr = info.get();
    indexes_[oid] = std::move(info);
    index_names_[index_name] = oid;
    return ptr;
}

IndexInfo* Catalog::GetIndex(const std::string& index_name) const {
    std::lock_guard<std::mutex> lock(catalog_mutex_);
    auto it = index_names_.find(index_name);
    if (it == index_names_.end()) {
        return nullptr;
    }
    return indexes_.at(it->second).get();
}

std::vector<IndexInfo*> Catalog::GetTableIndexes(const std::string& table_name) const {
    std::lock_guard<std::mutex> lock(catalog_mutex_);
    std::vector<IndexInfo*> res;
    for (const auto& [oid, info] : indexes_) {
        if (info->table_name == table_name) {
            res.push_back(info.get());
        }
    }
    return res;
}

std::vector<std::string> Catalog::GetTableNames() const {
    std::lock_guard<std::mutex> lock(catalog_mutex_);
    std::vector<std::string> names;
    for (const auto& [name, oid] : table_names_) {
        names.push_back(name);
    }
    return names;
}

} // namespace minidb
