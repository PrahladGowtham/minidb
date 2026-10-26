#pragma once

#include "common/config.hpp"
#include <string>
#include <mutex>
#include <vector>

namespace minidb {

/**
 * DiskManager takes care of the allocation and deallocation of physical pages on disk.
 * It performs block-level reads and writes directly using pread/pwrite.
 */
class DiskManager {
public:
    explicit DiskManager(const std::string& db_file);
    ~DiskManager();

    // Prevent copy
    DiskManager(const DiskManager&) = delete;
    DiskManager& operator=(const DiskManager&) = delete;

    /**
     * Write a 4096-byte page to disk.
     */
    void WritePage(page_id_t page_id, const char* page_data);

    /**
     * Read a 4096-byte page from disk into caller buffer.
     */
    void ReadPage(page_id_t page_id, char* page_data);

    /**
     * Allocate a new logical page and return its page_id.
     */
    page_id_t AllocatePage();

    /**
     * Deallocate a page, making its page ID eligible for reuse.
     */
    void DeallocatePage(page_id_t page_id);

    /**
     * Force synchronization of unwritten database file data to disk.
     */
    void Sync();

    /**
     * Returns total number of pages currently managed.
     */
    [[nodiscard]] size_t GetNumPages() const { return num_pages_; }

    /**
     * Returns total number of disk writes.
     */
    [[nodiscard]] size_t GetNumWrites() const { return num_writes_; }

    /**
     * Returns total number of disk reads.
     */
    [[nodiscard]] size_t GetNumReads() const { return num_reads_; }

private:
    std::string db_file_name_;
    int db_io_fd_{-1};
    size_t num_pages_{0};
    size_t num_writes_{0};
    size_t num_reads_{0};
    std::vector<page_id_t> free_pages_;
    mutable std::mutex db_io_mutex_;
};

} // namespace minidb
