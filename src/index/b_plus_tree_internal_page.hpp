#pragma once

#include "index/b_plus_tree_page.hpp"
#include <utility>
#include <algorithm>

namespace minidb {

/**
 * Store internal routing keys and child page_id pointers within an internal B+ Tree node.
 */
template <typename KeyType, typename KeyComparator>
class BPlusTreeInternalPage : public BPlusTreePage {
public:
    using MappingType = std::pair<KeyType, page_id_t>;

    void Init(page_id_t page_id, page_id_t parent_id = INVALID_PAGE_ID, int max_size = INTERNAL_PAGE_SLOT_COUNT) {
        SetPageType(IndexPageType::INTERNAL_PAGE);
        SetSize(0);
        SetMaxSize(max_size);
        SetParentPageId(parent_id);
        SetPageId(page_id);
    }

    const KeyType& KeyAt(int index) const { return array_[index].first; }
    void SetKeyAt(int index, const KeyType& key) { array_[index].first = key; }

    page_id_t ValueAt(int index) const { return array_[index].second; }
    void SetValueAt(int index, page_id_t value) { array_[index].second = value; }

    int ValueIndex(page_id_t value) const {
        for (int i = 0; i < GetSize(); ++i) {
            if (array_[i].second == value) {
                return i;
            }
        }
        return -1;
    }

    page_id_t Lookup(const KeyType& key, const KeyComparator& comparator) const {
        // Find largest i such that comparator(array_[i].first, key) <= 0
        int low = 1;
        int high = GetSize() - 1;
        int target_idx = 0;

        while (low <= high) {
            int mid = low + (high - low) / 2;
            if (comparator(array_[mid].first, key) <= 0) {
                target_idx = mid;
                low = mid + 1;
            } else {
                high = mid - 1;
            }
        }
        return array_[target_idx].second;
    }

    void Populate(page_id_t child0, const KeyType& key1, page_id_t child1) {
        array_[0].second = child0;
        array_[1] = {key1, child1};
        SetSize(2);
    }

    int InsertNodeAfter(page_id_t old_value, const KeyType& new_key, page_id_t new_value) {
        int idx = ValueIndex(old_value);
        if (idx == -1) {
            return GetSize();
        }

        int target_idx = idx + 1;
        for (int i = GetSize(); i > target_idx; --i) {
            array_[i] = array_[i - 1];
        }
        array_[target_idx] = {new_key, new_value};
        IncreaseSize(1);
        return GetSize();
    }

    void MoveHalfTo(BPlusTreeInternalPage* recipient, KeyType* middle_key) {
        int total = GetSize();
        int move_count = total / 2;
        int start_idx = total - move_count;

        *middle_key = array_[start_idx].first;

        for (int i = start_idx, j = 0; i < total; ++i, ++j) {
            recipient->array_[j] = array_[i];
        }
        recipient->SetSize(move_count);
        SetSize(start_idx);
    }

    void MoveAllTo(BPlusTreeInternalPage* recipient, const KeyType& middle_key) {
        int start_idx = recipient->GetSize();
        array_[0].first = middle_key;
        for (int i = 0; i < GetSize(); ++i) {
            recipient->array_[start_idx + i] = array_[i];
        }
        recipient->IncreaseSize(GetSize());
        SetSize(0);
    }

    void Remove(int index) {
        for (int i = index; i < GetSize() - 1; ++i) {
            array_[i] = array_[i + 1];
        }
        IncreaseSize(-1);
    }

    static constexpr size_t INTERNAL_PAGE_HEADER_SIZE = sizeof(BPlusTreePage);
    static constexpr size_t INTERNAL_PAGE_SLOT_COUNT = (PAGE_SIZE - INTERNAL_PAGE_HEADER_SIZE) / sizeof(MappingType);

private:
    MappingType array_[INTERNAL_PAGE_SLOT_COUNT];
};

} // namespace minidb
