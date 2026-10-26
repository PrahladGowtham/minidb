#include "recovery/log_manager.hpp"
#include "common/exception.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstring>

namespace minidb {

LogManager::LogManager(std::string wal_path) : wal_path_(std::move(wal_path)) {
    wal_fd_ = open(wal_path_.c_str(), O_RDWR | O_CREAT, 0644);
    if (wal_fd_ < 0) {
        throw DBException("Failed to open WAL file: " + wal_path_ + " (" + strerror(errno) + ")");
    }

    // Determine current next_lsn_ from existing WAL records if any
    auto existing_records = ReadAllRecords();
    if (!existing_records.empty()) {
        next_lsn_ = existing_records.back().GetLSN() + 1;
        flushed_lsn_ = existing_records.back().GetLSN();
    }
}

LogManager::~LogManager() {
    Flush();
    if (wal_fd_ >= 0) {
        close(wal_fd_);
        wal_fd_ = -1;
    }
}

lsn_t LogManager::AppendRecord(LogRecord* record) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    lsn_t lsn = next_lsn_++;
    record->SetLSN(lsn);

    record->Serialize(&log_buffer_);
    return lsn;
}

void LogManager::Flush() {
    std::lock_guard<std::mutex> lock(log_mutex_);
    if (log_buffer_.empty()) {
        return;
    }

    // Seek to end of WAL file
    lseek(wal_fd_, 0, SEEK_END);
    ssize_t written = write(wal_fd_, log_buffer_.data(), log_buffer_.size());
    if (written != static_cast<ssize_t>(log_buffer_.size())) {
        throw DBException("Failed to flush WAL log buffer: " + std::string(strerror(errno)));
    }

    fsync(wal_fd_);
    flushed_lsn_ = next_lsn_ - 1;
    log_buffer_.clear();
}

std::vector<LogRecord> LogManager::ReadAllRecords() {
    std::vector<LogRecord> records;
    if (wal_fd_ < 0) return records;

    struct stat st;
    if (fstat(wal_fd_, &st) != 0 || st.st_size == 0) {
        return records;
    }

    std::vector<char> file_data(st.st_size);
    ssize_t r = pread(wal_fd_, file_data.data(), st.st_size, 0);
    if (r <= 0) return records;

    size_t offset = 0;
    while (offset < static_cast<size_t>(r)) {
        LogRecord rec;
        if (!LogRecord::Deserialize(file_data.data() + offset, r - offset, &rec)) {
            break;
        }
        offset += rec.GetHeader().size;
        records.push_back(std::move(rec));
    }

    return records;
}

} // namespace minidb
