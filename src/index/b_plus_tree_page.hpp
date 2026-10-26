#pragma once

#include "common/config.hpp"
#include <cstdint>

namespace minidb {

enum class IndexPageType : uint32_t {
    INVALID_INDEX_PAGE = 0,
    LEAF_PAGE = 1,
    INTERNAL_PAGE = 2,
};

/**
 * Base class for B+ Tree leaf and internal pages stored inside 4096-byte Page frames.
 */
class BPlusTreePage {
public:
    [[nodiscard]] IndexPageType GetPageType() const { return page_type_; }
    void SetPageType(IndexPageType type) { page_type_ = type; }

    [[nodiscard]] int GetSize() const { return size_; }
    void SetSize(int size) { size_ = size; }
    void IncreaseSize(int amount) { size_ += amount; }

    [[nodiscard]] int GetMaxSize() const { return max_size_; }
    void SetMaxSize(int max_size) { max_size_ = max_size; }

    [[nodiscard]] int GetMinSize() const {
        if (IsLeafPage()) {
            return max_size_ / 2;
        }
        return (max_size_ + 1) / 2;
    }

    [[nodiscard]] page_id_t GetParentPageId() const { return parent_page_id_; }
    void SetParentPageId(page_id_t parent_page_id) { parent_page_id_ = parent_page_id; }

    [[nodiscard]] page_id_t GetPageId() const { return page_id_; }
    void SetPageId(page_id_t page_id) { page_id_ = page_id; }

    [[nodiscard]] lsn_t GetLSN() const { return lsn_; }
    void SetLSN(lsn_t lsn) { lsn_ = lsn; }

    [[nodiscard]] bool IsLeafPage() const { return page_type_ == IndexPageType::LEAF_PAGE; }
    [[nodiscard]] bool IsRootPage() const { return parent_page_id_ == INVALID_PAGE_ID; }

private:
    IndexPageType page_type_{IndexPageType::INVALID_INDEX_PAGE};
    lsn_t lsn_{INVALID_LSN};
    int size_{0};
    int max_size_{0};
    page_id_t parent_page_id_{INVALID_PAGE_ID};
    page_id_t page_id_{INVALID_PAGE_ID};
};

} // namespace minidb
