#pragma once

#include "common/config.hpp"
#include "storage/buffer/buffer_pool_manager.hpp"
#include "index/b_plus_tree_page.hpp"
#include "index/b_plus_tree_internal_page.hpp"
#include "index/b_plus_tree_leaf_page.hpp"
#include "index/b_plus_tree_iterator.hpp"

#include <string>
#include <vector>
#include <mutex>

namespace minidb {

/**
 * Disk-backed B+ Tree implementation supporting point lookups, insertions, splits,
 * deletions, and sequential range iterators.
 */
template <typename KeyType, typename ValueType, typename KeyComparator>
class BPlusTree {
public:
    using LeafPage = BPlusTreeLeafPage<KeyType, ValueType, KeyComparator>;
    using InternalPage = BPlusTreeInternalPage<KeyType, KeyComparator>;
    using Iterator = BPlusTreeIterator<KeyType, ValueType, KeyComparator>;

    explicit BPlusTree(std::string name, BufferPoolManager* bpm, const KeyComparator& comparator,
                       int leaf_max_size = LeafPage::LEAF_PAGE_SLOT_COUNT,
                       int internal_max_size = InternalPage::INTERNAL_PAGE_SLOT_COUNT)
        : name_(std::move(name)),
          bpm_(bpm),
          comparator_(comparator),
          leaf_max_size_(leaf_max_size),
          internal_max_size_(internal_max_size) {}

    [[nodiscard]] bool IsEmpty() const { return root_page_id_ == INVALID_PAGE_ID; }

    [[nodiscard]] page_id_t GetRootPageId() const { return root_page_id_; }
    void SetRootPageId(page_id_t root_id) { root_page_id_ = root_id; }

    /**
     * Point search for a key. Returns true if key exists.
     */
    bool GetValue(const KeyType& key, std::vector<ValueType>* result) {
        std::lock_guard<std::mutex> lock(tree_latch_);
        if (IsEmpty()) {
            return false;
        }

        Page* leaf_frame = FindLeafPage(key, false);
        if (leaf_frame == nullptr) {
            return false;
        }

        auto* leaf = reinterpret_cast<LeafPage*>(leaf_frame->GetData());
        ValueType val;
        bool found = leaf->Lookup(key, &val, comparator_);
        if (found && result != nullptr) {
            result->push_back(val);
        }

        bpm_->UnpinPage(leaf->GetPageId(), false);
        return found;
    }

    /**
     * Insert a key-value mapping into the B+ Tree.
     */
    bool Insert(const KeyType& key, const ValueType& value) {
        std::lock_guard<std::mutex> lock(tree_latch_);
        if (IsEmpty()) {
            StartNewTree(key, value);
            return true;
        }
        return InsertIntoLeaf(key, value);
    }

    /**
     * Delete a key from the B+ Tree.
     */
    void Remove(const KeyType& key) {
        std::lock_guard<std::mutex> lock(tree_latch_);
        if (IsEmpty()) {
            return;
        }

        Page* leaf_frame = FindLeafPage(key, false);
        if (leaf_frame == nullptr) {
            return;
        }

        auto* leaf = reinterpret_cast<LeafPage*>(leaf_frame->GetData());
        int size_before = leaf->GetSize();
        int size_after = leaf->RemoveAndDeleteRecord(key, comparator_);

        if (size_after < size_before) {
            if (leaf->IsRootPage()) {
                if (leaf->GetSize() == 0) {
                    root_page_id_ = INVALID_PAGE_ID;
                    bpm_->DeletePage(leaf->GetPageId());
                } else {
                    bpm_->UnpinPage(leaf->GetPageId(), true);
                }
                return;
            }

            // Update parent separator if first key changed
            if (leaf->GetSize() > 0) {
                UpdateParentKey(leaf);
            }

            if (leaf->GetSize() < leaf->GetMinSize()) {
                CoalesceOrRedistribute(leaf);
            } else {
                bpm_->UnpinPage(leaf->GetPageId(), true);
            }
        } else {
            bpm_->UnpinPage(leaf->GetPageId(), false);
        }
    }

    /**
     * Range Scan Iterators.
     */
    Iterator Begin() {
        std::lock_guard<std::mutex> lock(tree_latch_);
        if (IsEmpty()) {
            return End();
        }
        Page* leaf_frame = FindLeafPage(KeyType(), true);
        page_id_t leaf_page_id = leaf_frame->GetPageId();
        bpm_->UnpinPage(leaf_page_id, false);
        return Iterator(bpm_, leaf_page_id, 0);
    }

    Iterator Begin(const KeyType& key) {
        std::lock_guard<std::mutex> lock(tree_latch_);
        if (IsEmpty()) {
            return End();
        }
        Page* leaf_frame = FindLeafPage(key, false);
        auto* leaf = reinterpret_cast<LeafPage*>(leaf_frame->GetData());
        int idx = leaf->KeyIndex(key, comparator_);
        page_id_t leaf_page_id = leaf_frame->GetPageId();
        bpm_->UnpinPage(leaf_page_id, false);
        return Iterator(bpm_, leaf_page_id, idx);
    }

    Iterator End() {
        return Iterator(bpm_, INVALID_PAGE_ID, 0);
    }

private:
    void StartNewTree(const KeyType& key, const ValueType& value) {
        page_id_t new_page_id = INVALID_PAGE_ID;
        Page* root_page = bpm_->NewPage(&new_page_id);
        if (root_page == nullptr) {
            return;
        }

        auto* leaf = reinterpret_cast<LeafPage*>(root_page->GetData());
        leaf->Init(new_page_id, INVALID_PAGE_ID, leaf_max_size_);
        leaf->Insert(key, value, comparator_);

        root_page_id_ = new_page_id;
        bpm_->UnpinPage(new_page_id, true);
    }

    bool InsertIntoLeaf(const KeyType& key, const ValueType& value) {
        Page* leaf_frame = FindLeafPage(key, false);
        auto* leaf = reinterpret_cast<LeafPage*>(leaf_frame->GetData());

        if (leaf->GetSize() < leaf->GetMaxSize()) {
            leaf->Insert(key, value, comparator_);
            bpm_->UnpinPage(leaf->GetPageId(), true);
            return true;
        }

        // Split is required
        page_id_t new_page_id = INVALID_PAGE_ID;
        Page* new_frame = bpm_->NewPage(&new_page_id);
        if (new_frame == nullptr) {
            bpm_->UnpinPage(leaf->GetPageId(), false);
            return false;
        }

        auto* new_leaf = reinterpret_cast<LeafPage*>(new_frame->GetData());
        new_leaf->Init(new_page_id, leaf->GetParentPageId(), leaf_max_size_);

        leaf->MoveHalfTo(new_leaf);

        if (comparator_(key, new_leaf->KeyAt(0)) < 0) {
            leaf->Insert(key, value, comparator_);
        } else {
            new_leaf->Insert(key, value, comparator_);
        }

        InsertIntoParent(leaf, new_leaf->KeyAt(0), new_leaf);

        bpm_->UnpinPage(leaf->GetPageId(), true);
        bpm_->UnpinPage(new_leaf->GetPageId(), true);
        return true;
    }

    void InsertIntoParent(BPlusTreePage* old_node, const KeyType& key, BPlusTreePage* new_node) {
        if (old_node->IsRootPage()) {
            page_id_t new_root_id = INVALID_PAGE_ID;
            Page* new_root_frame = bpm_->NewPage(&new_root_id);
            auto* new_root = reinterpret_cast<InternalPage*>(new_root_frame->GetData());

            new_root->Init(new_root_id, INVALID_PAGE_ID, internal_max_size_);
            new_root->Populate(old_node->GetPageId(), key, new_node->GetPageId());

            old_node->SetParentPageId(new_root_id);
            new_node->SetParentPageId(new_root_id);

            root_page_id_ = new_root_id;
            bpm_->UnpinPage(new_root_id, true);
            return;
        }

        page_id_t parent_id = old_node->GetParentPageId();
        Page* parent_frame = bpm_->FetchPage(parent_id);
        auto* parent = reinterpret_cast<InternalPage*>(parent_frame->GetData());

        if (parent->GetSize() < parent->GetMaxSize()) {
            parent->InsertNodeAfter(old_node->GetPageId(), key, new_node->GetPageId());
            new_node->SetParentPageId(parent_id);
            bpm_->UnpinPage(parent_id, true);
            return;
        }

        // Split internal node
        page_id_t new_internal_id = INVALID_PAGE_ID;
        Page* new_internal_frame = bpm_->NewPage(&new_internal_id);
        auto* new_internal = reinterpret_cast<InternalPage*>(new_internal_frame->GetData());
        new_internal->Init(new_internal_id, parent->GetParentPageId(), internal_max_size_);

        KeyType middle_key;
        parent->MoveHalfTo(new_internal, &middle_key);

        if (comparator_(key, middle_key) < 0) {
            parent->InsertNodeAfter(old_node->GetPageId(), key, new_node->GetPageId());
            new_node->SetParentPageId(parent->GetPageId());
        } else {
            new_internal->InsertNodeAfter(old_node->GetPageId(), key, new_node->GetPageId());
            new_node->SetParentPageId(new_internal->GetPageId());
        }

        // Update child pages to point to new_internal parent
        for (int i = 0; i < new_internal->GetSize(); ++i) {
            Page* child_frame = bpm_->FetchPage(new_internal->ValueAt(i));
            auto* child = reinterpret_cast<BPlusTreePage*>(child_frame->GetData());
            child->SetParentPageId(new_internal->GetPageId());
            bpm_->UnpinPage(child->GetPageId(), true);
        }

        InsertIntoParent(parent, middle_key, new_internal);

        bpm_->UnpinPage(parent->GetPageId(), true);
        bpm_->UnpinPage(new_internal->GetPageId(), true);
    }

    void UpdateParentKey(LeafPage* node) {
        if (!node->IsRootPage() && node->GetSize() > 0) {
            Page* parent_frame = bpm_->FetchPage(node->GetParentPageId());
            auto* parent = reinterpret_cast<InternalPage*>(parent_frame->GetData());
            int idx = parent->ValueIndex(node->GetPageId());
            if (idx > 0) {
                parent->SetKeyAt(idx, node->KeyAt(0));
                bpm_->UnpinPage(parent->GetPageId(), true);
            } else {
                bpm_->UnpinPage(parent->GetPageId(), false);
            }
        }
    }

    void CoalesceOrRedistribute(LeafPage* node) {
        Page* parent_frame = bpm_->FetchPage(node->GetParentPageId());
        auto* parent = reinterpret_cast<InternalPage*>(parent_frame->GetData());
        int node_idx = parent->ValueIndex(node->GetPageId());

        // Select sibling
        int sibling_idx = (node_idx == 0) ? 1 : node_idx - 1;
        page_id_t sibling_page_id = parent->ValueAt(sibling_idx);
        Page* sibling_frame = bpm_->FetchPage(sibling_page_id);
        auto* sibling = reinterpret_cast<LeafPage*>(sibling_frame->GetData());

        LeafPage* left_leaf = (node_idx == 0) ? node : sibling;
        LeafPage* right_leaf = (node_idx == 0) ? sibling : node;
        int right_idx = (node_idx == 0) ? sibling_idx : node_idx;

        if (left_leaf->GetSize() + right_leaf->GetSize() <= left_leaf->GetMaxSize()) {
            // Coalesce: merge right into left
            right_leaf->MoveAllTo(left_leaf);
            left_leaf->SetNextPageId(right_leaf->GetNextPageId());

            if (right_leaf->GetNextPageId() != INVALID_PAGE_ID) {
                Page* next_frame = bpm_->FetchPage(right_leaf->GetNextPageId());
                auto* next_leaf = reinterpret_cast<LeafPage*>(next_frame->GetData());
                next_leaf->SetPrevPageId(left_leaf->GetPageId());
                bpm_->UnpinPage(next_leaf->GetPageId(), true);
            }

            parent->Remove(right_idx);
            page_id_t right_page_id = right_leaf->GetPageId();

            bpm_->UnpinPage(left_leaf->GetPageId(), true);
            bpm_->DeletePage(right_page_id);

            // Check if parent root needs shrinking
            if (parent->IsRootPage()) {
                if (parent->GetSize() == 1) {
                    root_page_id_ = parent->ValueAt(0);
                    Page* new_root_frame = bpm_->FetchPage(root_page_id_);
                    auto* new_root = reinterpret_cast<BPlusTreePage*>(new_root_frame->GetData());
                    new_root->SetParentPageId(INVALID_PAGE_ID);
                    bpm_->UnpinPage(root_page_id_, true);
                    bpm_->DeletePage(parent->GetPageId());
                } else {
                    bpm_->UnpinPage(parent->GetPageId(), true);
                }
            } else {
                bpm_->UnpinPage(parent->GetPageId(), true);
            }
        } else {
            // Redistribution
            if (node_idx == 0) {
                // Borrow first from right sibling
                right_leaf->MoveFirstToEndOf(left_leaf);
                parent->SetKeyAt(right_idx, right_leaf->KeyAt(0));
            } else {
                // Borrow last from left sibling
                left_leaf->MoveLastToFrontOf(right_leaf);
                parent->SetKeyAt(node_idx, right_leaf->KeyAt(0));
            }

            bpm_->UnpinPage(left_leaf->GetPageId(), true);
            bpm_->UnpinPage(right_leaf->GetPageId(), true);
            bpm_->UnpinPage(parent->GetPageId(), true);
        }
    }

    Page* FindLeafPage(const KeyType& key, bool left_most) {
        if (root_page_id_ == INVALID_PAGE_ID) {
            return nullptr;
        }

        page_id_t curr_page_id = root_page_id_;
        Page* curr_frame = bpm_->FetchPage(curr_page_id);
        auto* curr_page = reinterpret_cast<BPlusTreePage*>(curr_frame->GetData());

        while (!curr_page->IsLeafPage()) {
            auto* internal = reinterpret_cast<InternalPage*>(curr_page);
            page_id_t next_page_id = left_most ? internal->ValueAt(0) : internal->Lookup(key, comparator_);

            bpm_->UnpinPage(curr_page_id, false);
            curr_page_id = next_page_id;
            curr_frame = bpm_->FetchPage(curr_page_id);
            curr_page = reinterpret_cast<BPlusTreePage*>(curr_frame->GetData());
        }

        return curr_frame;
    }

    std::string name_;
    page_id_t root_page_id_{INVALID_PAGE_ID};
    BufferPoolManager* bpm_;
    KeyComparator comparator_;
    int leaf_max_size_;
    int internal_max_size_;
    std::mutex tree_latch_;
};

} // namespace minidb
