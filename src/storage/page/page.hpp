#pragma once

#include "common/config.hpp"
#include <cstring>
#include <shared_mutex>

namespace minidb {

/**
 * Page represents an in-memory frame in the Buffer Pool.
 * It holds the raw 4096-byte content and metadata such as pin count and dirty flag.
 */
class Page {
    friend class BufferPoolManager;
public:
    Page() { ResetMemory(); }
    ~Page() = default;

    // Direct access to page data
    [[nodiscard]] char* GetData() { return data_; }
    [[nodiscard]] const char* GetData() const { return data_; }

    [[nodiscard]] page_id_t GetPageId() const { return page_id_; }
    [[nodiscard]] int GetPinCount() const { return pin_count_; }
    [[nodiscard]] bool IsDirty() const { return is_dirty_; }
    [[nodiscard]] lsn_t GetPageLSN() const { return page_lsn_; }

    void SetPageLSN(lsn_t lsn) { page_lsn_ = lsn; }

    // Concurrency control latches
    void WLatch() { rwlatch_.lock(); }
    void WUnlatch() { rwlatch_.unlock(); }
    void RLatch() { rwlatch_.lock_shared(); }
    void RUnlatch() { rwlatch_.unlock_shared(); }

private:
    void ResetMemory() {
        std::memset(data_, 0, PAGE_SIZE);
        page_id_ = INVALID_PAGE_ID;
        pin_count_ = 0;
        is_dirty_ = false;
        page_lsn_ = INVALID_LSN;
    }

    char data_[PAGE_SIZE];
    page_id_t page_id_{INVALID_PAGE_ID};
    int pin_count_{0};
    bool is_dirty_{false};
    lsn_t page_lsn_{INVALID_LSN};
    std::shared_mutex rwlatch_;
};

} // namespace minidb
