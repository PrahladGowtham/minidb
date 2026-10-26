#pragma once

#include "catalog/catalog.hpp"
#include "execution/execution_engine.hpp"
#include "recovery/log_manager.hpp"
#include "storage/buffer/buffer_pool_manager.hpp"
#include "storage/disk/disk_manager.hpp"

#include <memory>
#include <string>

namespace minidb {

/**
 * Top-level Database facade managing the lifecycle of all database subsystems.
 */
class Database {
public:
    static std::unique_ptr<Database> Open(const std::string& db_path, size_t pool_size = 64);
    ~Database();

    QueryResult Execute(const std::string& sql);
    [[nodiscard]] std::vector<std::string> GetTableNames() const;
    [[nodiscard]] const TableInfo* GetTable(const std::string& table_name) const;
    void Flush();

private:
    explicit Database(const std::string& db_path, size_t pool_size);

    std::string db_path_;
    std::unique_ptr<DiskManager> disk_manager_;
    std::unique_ptr<BufferPoolManager> buffer_pool_manager_;
    std::unique_ptr<LogManager> log_manager_;
    std::unique_ptr<Catalog> catalog_;
    std::unique_ptr<ExecutionEngine> engine_;
};

} // namespace minidb

// Pure C ABI for embedding
extern "C" {
    typedef struct minidb_handle minidb_t;
    minidb_t* minidb_open(const char* path);
    char* minidb_execute(minidb_t* db, const char* sql);
    void minidb_free_string(char* str);
    void minidb_close(minidb_t* db);
}
