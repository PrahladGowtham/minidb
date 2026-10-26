#include "api/minidb.hpp"
#include <cstring>
#include <cstdlib>

namespace minidb {

std::unique_ptr<Database> Database::Open(const std::string& db_path, size_t pool_size) {
    return std::unique_ptr<Database>(new Database(db_path, pool_size));
}

Database::Database(const std::string& db_path, size_t pool_size) : db_path_(db_path) {
    disk_manager_ = std::make_unique<DiskManager>(db_path_);
    buffer_pool_manager_ = std::make_unique<BufferPoolManager>(pool_size, disk_manager_.get());
    log_manager_ = std::make_unique<LogManager>(db_path_ + ".wal");
    catalog_ = std::make_unique<Catalog>(buffer_pool_manager_.get());
    engine_ = std::make_unique<ExecutionEngine>(catalog_.get(), buffer_pool_manager_.get());
}

Database::~Database() {
    Flush();
}

QueryResult Database::Execute(const std::string& sql) {
    return engine_->Execute(sql);
}

std::vector<std::string> Database::GetTableNames() const {
    return catalog_->GetTableNames();
}

const TableInfo* Database::GetTable(const std::string& table_name) const {
    return catalog_->GetTable(table_name);
}

void Database::Flush() {
    if (log_manager_) {
        log_manager_->Flush();
    }
    if (buffer_pool_manager_) {
        buffer_pool_manager_->FlushAllPages();
    }
    if (disk_manager_) {
        disk_manager_->Sync();
    }
}

} // namespace minidb

// C ABI Implementation
struct minidb_handle {
    std::unique_ptr<minidb::Database> db;
};

extern "C" {

minidb_t* minidb_open(const char* path) {
    try {
        auto handle = new minidb_handle();
        handle->db = minidb::Database::Open(path ? path : "minidb.db");
        return handle;
    } catch (...) {
        return nullptr;
    }
}

char* minidb_execute(minidb_t* db, const char* sql) {
    if (!db || !db->db || !sql) return nullptr;
    auto res = db->db->Execute(sql);
    std::string out = res.success ? ("OK: " + res.message) : ("ERROR: " + res.message);
    char* cstr = static_cast<char*>(malloc(out.size() + 1));
    std::memcpy(cstr, out.c_str(), out.size() + 1);
    return cstr;
}

void minidb_free_string(char* str) {
    free(str);
}

void minidb_close(minidb_t* db) {
    delete db;
}

}
