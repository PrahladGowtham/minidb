#pragma once

#include "common/config.hpp"
#include "common/rid.hpp"
#include "catalog/tuple.hpp"
#include "storage/buffer/buffer_pool_manager.hpp"
#include "storage/page/slotted_page.hpp"
#include <mutex>

namespace minidb {

class TableIterator;

/**
 * TableHeap represents a physical relational table stored across a linked sequence of slotted pages.
 */
class TableHeap {
    friend class TableIterator;
public:
    explicit TableHeap(BufferPoolManager* bpm, page_id_t first_page_id = INVALID_PAGE_ID);

    bool InsertTuple(const Tuple& tuple, RID* out_rid);
    bool GetTuple(const RID& rid, Tuple* out_tuple);
    bool UpdateTuple(const Tuple& tuple, const RID& rid);
    bool DeleteTuple(const RID& rid);

    [[nodiscard]] page_id_t GetFirstPageId() const { return first_page_id_; }

    TableIterator Begin(const Schema* schema);
    TableIterator End();

private:
    BufferPoolManager* bpm_;
    page_id_t first_page_id_{INVALID_PAGE_ID};
    std::mutex heap_mutex_;
};

class TableIterator {
public:
    TableIterator() = default;
    TableIterator(TableHeap* table_heap, RID rid);

    const Tuple& operator*() const { return current_tuple_; }
    const Tuple* operator->() const { return &current_tuple_; }

    TableIterator& operator++();

    bool operator==(const TableIterator& other) const {
        return rid_ == other.rid_;
    }

    bool operator!=(const TableIterator& other) const {
        return !(*this == other);
    }

private:
    void Advance();

    TableHeap* table_heap_{nullptr};
    RID rid_;
    Tuple current_tuple_;
};

} // namespace minidb
