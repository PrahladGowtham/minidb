#pragma once

#include "storage/buffer/buffer_pool_manager.hpp"
#include "index/b_plus_tree_leaf_page.hpp"

namespace minidb {

template <typename KeyType, typename ValueType, typename KeyComparator>
class BPlusTreeIterator {
public:
    using LeafPage = BPlusTreeLeafPage<KeyType, ValueType, KeyComparator>;
    using MappingType = std::pair<KeyType, ValueType>;

    BPlusTreeIterator() = default;

    BPlusTreeIterator(BufferPoolManager* bpm, page_id_t page_id, int index = 0)
        : bpm_(bpm), current_page_id_(page_id), index_(index) {
        if (current_page_id_ != INVALID_PAGE_ID && bpm_ != nullptr) {
            current_page_ = bpm_->FetchPage(current_page_id_);
            if (current_page_ != nullptr) {
                auto* leaf = reinterpret_cast<LeafPage*>(current_page_->GetData());
                if (index_ >= leaf->GetSize()) {
                    Advance();
                }
            }
        }
    }

    ~BPlusTreeIterator() {
        Clear();
    }

    // Move semantics
    BPlusTreeIterator(BPlusTreeIterator&& other) noexcept
        : bpm_(other.bpm_),
          current_page_id_(other.current_page_id_),
          index_(other.index_),
          current_page_(other.current_page_) {
        other.bpm_ = nullptr;
        other.current_page_id_ = INVALID_PAGE_ID;
        other.index_ = 0;
        other.current_page_ = nullptr;
    }

    BPlusTreeIterator& operator=(BPlusTreeIterator&& other) noexcept {
        if (this != &other) {
            Clear();
            bpm_ = other.bpm_;
            current_page_id_ = other.current_page_id_;
            index_ = other.index_;
            current_page_ = other.current_page_;
            other.bpm_ = nullptr;
            other.current_page_id_ = INVALID_PAGE_ID;
            other.index_ = 0;
            other.current_page_ = nullptr;
        }
        return *this;
    }

    // Prevent implicit copy to avoid pin count mismatch
    BPlusTreeIterator(const BPlusTreeIterator&) = delete;
    BPlusTreeIterator& operator=(const BPlusTreeIterator&) = delete;

    [[nodiscard]] bool IsEnd() const {
        return current_page_id_ == INVALID_PAGE_ID || current_page_ == nullptr;
    }

    const MappingType& operator*() const {
        auto* leaf = reinterpret_cast<const LeafPage*>(current_page_->GetData());
        return leaf->GetItem(index_);
    }

    BPlusTreeIterator& operator++() {
        index_++;
        auto* leaf = reinterpret_cast<LeafPage*>(current_page_->GetData());
        if (index_ >= leaf->GetSize()) {
            Advance();
        }
        return *this;
    }

    bool operator==(const BPlusTreeIterator& other) const {
        if (IsEnd() && other.IsEnd()) {
            return true;
        }
        return current_page_id_ == other.current_page_id_ && index_ == other.index_;
    }

    bool operator!=(const BPlusTreeIterator& other) const {
        return !(*this == other);
    }

private:
    void Advance() {
        if (current_page_ == nullptr) {
            return;
        }
        auto* leaf = reinterpret_cast<LeafPage*>(current_page_->GetData());
        page_id_t next_page_id = leaf->GetNextPageId();

        bpm_->UnpinPage(current_page_id_, false);
        current_page_ = nullptr;
        current_page_id_ = next_page_id;
        index_ = 0;

        if (current_page_id_ != INVALID_PAGE_ID) {
            current_page_ = bpm_->FetchPage(current_page_id_);
        }
    }

    void Clear() {
        if (current_page_ != nullptr && bpm_ != nullptr && current_page_id_ != INVALID_PAGE_ID) {
            bpm_->UnpinPage(current_page_id_, false);
            current_page_ = nullptr;
            current_page_id_ = INVALID_PAGE_ID;
        }
    }

    BufferPoolManager* bpm_{nullptr};
    page_id_t current_page_id_{INVALID_PAGE_ID};
    int index_{0};
    Page* current_page_{nullptr};
};

} // namespace minidb
