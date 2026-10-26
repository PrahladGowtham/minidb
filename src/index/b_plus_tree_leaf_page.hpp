#pragma once

#include "index/b_plus_tree_page.hpp"
#include "common/rid.hpp"
#include <utility>
#include <algorithm>

namespace minidb {

/**
 * Store indexed key and value (RID) within a leaf node page.
 */
template <typename KeyType, typename ValueType, typename KeyComparator>
class BPlusTreeLeafPage : public BPlusTreePage {
public:
    using MappingType = std::pair<KeyType, ValueType>;

    void Init(page_id_t page_id, page_id_t parent_id = INVALID_PAGE_ID, int max_size = LEAF_PAGE_SLOT_COUNT) {
        SetPageType(IndexPageType::LEAF_PAGE);
        SetSize(0);
        SetMaxSize(max_size);
        SetParentPageId(parent_id);
        SetPageId(page_id);
        next_page_id_ = INVALID_PAGE_ID;
        prev_page_id_ = INVALID_PAGE_ID;
    }

    [[nodiscard]] page_id_t GetNextPageId() const { return next_page_id_; }
    void SetNextPageId(page_id_t next_page_id) { next_page_id_ = next_page_id; }

    [[nodiscard]] page_id_t GetPrevPageId() const { return prev_page_id_; }
    void SetPrevPageId(page_id_t prev_page_id) { prev_page_id_ = prev_page_id; }

    const KeyType& KeyAt(int index) const { return array_[index].first; }
    const ValueType& ValueAt(int index) const { return array_[index].second; }
    const MappingType& GetItem(int index) const { return array_[index]; }

    int KeyIndex(const KeyType& key, const KeyComparator& comparator) const {
        int low = 0;
        int high = GetSize() - 1;
        while (low <= high) {
            int mid = low + (high - low) / 2;
            int comp = comparator(array_[mid].first, key);
            if (comp < 0) {
                low = mid + 1;
            } else {
                high = mid - 1;
            }
        }
        return low;
    }

    bool Lookup(const KeyType& key, ValueType* result, const KeyComparator& comparator) const {
        int idx = KeyIndex(key, comparator);
        if (idx < GetSize() && comparator(array_[idx].first, key) == 0) {
            if (result) {
                *result = array_[idx].second;
            }
            return true;
        }
        return false;
    }

    int Insert(const KeyType& key, const ValueType& value, const KeyComparator& comparator) {
        int idx = KeyIndex(key, comparator);
        if (idx < GetSize() && comparator(array_[idx].first, key) == 0) {
            // Key already exists (unique index default)
            array_[idx].second = value;
            return GetSize();
        }

        for (int i = GetSize(); i > idx; --i) {
            array_[i] = array_[i - 1];
        }
        array_[idx] = {key, value};
        IncreaseSize(1);
        return GetSize();
    }

    void MoveHalfTo(BPlusTreeLeafPage* recipient) {
        int total = GetSize();
        int move_count = total / 2;
        int start_idx = total - move_count;

        for (int i = start_idx, j = 0; i < total; ++i, ++j) {
            recipient->array_[j] = array_[i];
        }
        recipient->SetSize(move_count);
        SetSize(start_idx);

        recipient->SetNextPageId(next_page_id_);
        recipient->SetPrevPageId(GetPageId());
        SetNextPageId(recipient->GetPageId());
    }

    void MoveAllTo(BPlusTreeLeafPage* recipient) {
        int start_idx = recipient->GetSize();
        for (int i = 0; i < GetSize(); ++i) {
            recipient->array_[start_idx + i] = array_[i];
        }
        recipient->IncreaseSize(GetSize());
        recipient->SetNextPageId(next_page_id_);
        SetSize(0);
    }

    int RemoveAndDeleteRecord(const KeyType& key, const KeyComparator& comparator) {
        int idx = KeyIndex(key, comparator);
        if (idx < GetSize() && comparator(array_[idx].first, key) == 0) {
            for (int i = idx; i < GetSize() - 1; ++i) {
                array_[i] = array_[i + 1];
            }
            IncreaseSize(-1);
        }
        return GetSize();
    }

    void MoveFirstToEndOf(BPlusTreeLeafPage* recipient) {
        recipient->CopyLastFrom(array_[0]);
        for (int i = 0; i < GetSize() - 1; ++i) {
            array_[i] = array_[i + 1];
        }
        IncreaseSize(-1);
    }

    void CopyLastFrom(const MappingType& item) {
        array_[GetSize()] = item;
        IncreaseSize(1);
    }

    void MoveLastToFrontOf(BPlusTreeLeafPage* recipient) {
        recipient->CopyFirstFrom(array_[GetSize() - 1]);
        IncreaseSize(-1);
    }

    void CopyFirstFrom(const MappingType& item) {
        for (int i = GetSize(); i > 0; --i) {
            array_[i] = array_[i - 1];
        }
        array_[0] = item;
        IncreaseSize(1);
    }

    static constexpr size_t LEAF_PAGE_HEADER_SIZE = sizeof(BPlusTreePage) + sizeof(page_id_t) * 2;
    static constexpr size_t LEAF_PAGE_SLOT_COUNT = (PAGE_SIZE - LEAF_PAGE_HEADER_SIZE) / sizeof(MappingType);

private:
    page_id_t next_page_id_{INVALID_PAGE_ID};
    page_id_t prev_page_id_{INVALID_PAGE_ID};
    MappingType array_[LEAF_PAGE_SLOT_COUNT];
};

} // namespace minidb
