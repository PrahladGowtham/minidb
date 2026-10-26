#pragma once

#include "recovery/log_record.hpp"
#include <string>
#include <mutex>
#include <vector>

namespace minidb {

class LogManager {
public:
    explicit LogManager(std::string wal_path);
    ~LogManager();

    lsn_t AppendRecord(LogRecord* record);
    void Flush();

    [[nodiscard]] lsn_t GetNextLSN() const { return next_lsn_; }
    [[nodiscard]] lsn_t GetFlushedLSN() const { return flushed_lsn_; }

    std::vector<LogRecord> ReadAllRecords();

private:
    std::string wal_path_;
    int wal_fd_{-1};
    lsn_t next_lsn_{0};
    lsn_t flushed_lsn_{INVALID_LSN};

    std::vector<char> log_buffer_;
    std::mutex log_mutex_;
};

} // namespace minidb
