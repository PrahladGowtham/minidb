#include "storage/disk/disk_manager.hpp"
#include "common/exception.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstring>

namespace minidb {

DiskManager::DiskManager(const std::string& db_file) : db_file_name_(db_file) {
    db_io_fd_ = open(db_file.c_str(), O_RDWR | O_CREAT, 0644);
    if (db_io_fd_ < 0) {
        throw DBException("Failed to open or create database file: " + db_file + " (" + strerror(errno) + ")");
    }

    struct stat file_stat;
    if (fstat(db_io_fd_, &file_stat) == 0) {
        num_pages_ = file_stat.st_size / PAGE_SIZE;
    }
}

DiskManager::~DiskManager() {
    if (db_io_fd_ >= 0) {
        Sync();
        close(db_io_fd_);
        db_io_fd_ = -1;
    }
}

void DiskManager::WritePage(page_id_t page_id, const char* page_data) {
    std::lock_guard<std::mutex> lock(db_io_mutex_);
    off_t offset = static_cast<off_t>(page_id) * PAGE_SIZE;

    ssize_t bytes_written = pwrite(db_io_fd_, page_data, PAGE_SIZE, offset);
    if (bytes_written != static_cast<ssize_t>(PAGE_SIZE)) {
        throw DBException("DiskManager::WritePage failed for page_id " + std::to_string(page_id) +
                          ": " + strerror(errno));
    }
    num_writes_++;
}

void DiskManager::ReadPage(page_id_t page_id, char* page_data) {
    std::lock_guard<std::mutex> lock(db_io_mutex_);
    off_t offset = static_cast<off_t>(page_id) * PAGE_SIZE;

    ssize_t bytes_read = pread(db_io_fd_, page_data, PAGE_SIZE, offset);
    if (bytes_read < 0) {
        throw DBException("DiskManager::ReadPage failed for page_id " + std::to_string(page_id) +
                          ": " + strerror(errno));
    }

    // If reading past EOF or partial page, zero out the remaining bytes
    if (bytes_read < static_cast<ssize_t>(PAGE_SIZE)) {
        std::memset(page_data + bytes_read, 0, PAGE_SIZE - bytes_read);
    }
    num_reads_++;
}

page_id_t DiskManager::AllocatePage() {
    std::lock_guard<std::mutex> lock(db_io_mutex_);
    if (!free_pages_.empty()) {
        page_id_t recycled_id = free_pages_.back();
        free_pages_.pop_back();
        return recycled_id;
    }
    page_id_t new_page_id = static_cast<page_id_t>(num_pages_++);
    return new_page_id;
}

void DiskManager::DeallocatePage(page_id_t page_id) {
    std::lock_guard<std::mutex> lock(db_io_mutex_);
    free_pages_.push_back(page_id);
}

void DiskManager::Sync() {
    std::lock_guard<std::mutex> lock(db_io_mutex_);
    if (db_io_fd_ >= 0) {
        fsync(db_io_fd_);
    }
}

} // namespace minidb
