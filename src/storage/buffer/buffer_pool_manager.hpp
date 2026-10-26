#pragma once

#include "common/config.hpp"
#include "storage/disk/disk_manager.hpp"
#include "storage/page/page.hpp"
#include "storage/buffer/replacer.hpp"

#include <memory>
#include <vector>
#include <unordered_map>
#include <list>
#include <mutex>

namespace minidb {

/**
 * BufferPoolManager coordinates caching disk pages in RAM.
 */
class BufferPoolManager {
public:
    BufferPoolManager(size_t pool_size, DiskManager* disk_manager);
    ~BufferPoolManager();

    // Disable copy
    BufferPoolManager(const BufferPoolManager&) = delete;
    BufferPoolManager& operator=(const BufferPoolManager&) = delete;

    /**
     * Fetch the requested page from the buffer pool.
     */
    Page* FetchPage(page_id_t page_id);

    /**
     * Unpin a page in the buffer pool.
     */
    bool UnpinPage(page_id_t page_id, bool is_dirty);

    /**
     * Flushes the target page to disk.
     */
    bool FlushPage(page_id_t page_id);

    /**
     * Creates a new page in the buffer pool.
     */
    Page* NewPage(page_id_t* page_id);

    /**
     * Deletes a page from the buffer pool and disk.
     */
    bool DeletePage(page_id_t page_id);

    /**
     * Flushes all pages to disk.
     */
    void FlushAllPages();

    [[nodiscard]] size_t GetPoolSize() const { return pool_size_; }

private:
    /**
     * Helper to find a replacement frame from free_list or replacer.
     */
    bool FindAvailableFrame(frame_id_t* frame_id);

    size_t pool_size_;
    DiskManager* disk_manager_;
    std::unique_ptr<Page[]> pages_;
    std::unique_ptr<Replacer> replacer_;
    std::unordered_map<page_id_t, frame_id_t> page_table_;
    std::list<frame_id_t> free_list_;
    std::mutex mutex_;
};

} // namespace minidb
