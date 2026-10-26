#include "storage/table/table_heap.hpp"

namespace minidb {

TableHeap::TableHeap(BufferPoolManager* bpm, page_id_t first_page_id)
    : bpm_(bpm), first_page_id_(first_page_id) {
    if (first_page_id_ == INVALID_PAGE_ID) {
        Page* page = bpm_->NewPage(&first_page_id_);
        if (page != nullptr) {
            SlottedPage slotted(page->GetData());
            slotted.Init(first_page_id_);
            bpm_->UnpinPage(first_page_id_, true);
        }
    }
}

bool TableHeap::InsertTuple(const Tuple& tuple, RID* out_rid) {
    std::lock_guard<std::mutex> lock(heap_mutex_);

    page_id_t curr_page_id = first_page_id_;
    Page* curr_frame = bpm_->FetchPage(curr_page_id);
    if (curr_frame == nullptr) return false;

    SlottedPage curr_page(curr_frame->GetData());

    // Traverse to tail of table heap
    while (curr_page.GetNextPageId() != INVALID_PAGE_ID) {
        page_id_t next_pid = curr_page.GetNextPageId();
        bpm_->UnpinPage(curr_page_id, false);
        curr_page_id = next_pid;
        curr_frame = bpm_->FetchPage(curr_page_id);
        curr_page = SlottedPage(curr_frame->GetData());
    }

    slot_id_t slot_id = INVALID_SLOT_ID;
    if (curr_page.InsertRecord(tuple.GetData(), tuple.GetLength(), &slot_id)) {
        *out_rid = RID(curr_page_id, slot_id);
        bpm_->UnpinPage(curr_page_id, true);
        return true;
    }

    // Current page is full: allocate a new slotted page
    page_id_t new_page_id = INVALID_PAGE_ID;
    Page* new_frame = bpm_->NewPage(&new_page_id);
    if (new_frame == nullptr) {
        bpm_->UnpinPage(curr_page_id, false);
        return false;
    }

    SlottedPage new_page(new_frame->GetData());
    new_page.Init(new_page_id, curr_page_id, INVALID_PAGE_ID);
    curr_page.SetNextPageId(new_page_id);

    bool inserted = new_page.InsertRecord(tuple.GetData(), tuple.GetLength(), &slot_id);
    *out_rid = RID(new_page_id, slot_id);

    bpm_->UnpinPage(curr_page_id, true);
    bpm_->UnpinPage(new_page_id, true);

    return inserted;
}

bool TableHeap::GetTuple(const RID& rid, Tuple* out_tuple) {
    if (!rid.IsValid()) return false;

    Page* frame = bpm_->FetchPage(rid.GetPageId());
    if (frame == nullptr) return false;

    SlottedPage page(frame->GetData());
    std::string_view record_data;
    bool found = page.GetRecord(rid.GetSlotId(), &record_data);

    if (found && out_tuple != nullptr) {
        *out_tuple = Tuple(record_data.data(), static_cast<uint32_t>(record_data.size()), rid);
    }

    bpm_->UnpinPage(rid.GetPageId(), false);
    return found;
}

bool TableHeap::UpdateTuple(const Tuple& tuple, const RID& rid) {
    if (!rid.IsValid()) return false;
    std::lock_guard<std::mutex> lock(heap_mutex_);

    Page* frame = bpm_->FetchPage(rid.GetPageId());
    if (frame == nullptr) return false;

    SlottedPage page(frame->GetData());
    bool ok = page.UpdateRecord(rid.GetSlotId(), tuple.GetData(), tuple.GetLength());
    bpm_->UnpinPage(rid.GetPageId(), ok);
    return ok;
}

bool TableHeap::DeleteTuple(const RID& rid) {
    if (!rid.IsValid()) return false;
    std::lock_guard<std::mutex> lock(heap_mutex_);

    Page* frame = bpm_->FetchPage(rid.GetPageId());
    if (frame == nullptr) return false;

    SlottedPage page(frame->GetData());
    bool ok = page.DeleteRecord(rid.GetSlotId());
    bpm_->UnpinPage(rid.GetPageId(), ok);
    return ok;
}

TableIterator TableHeap::Begin(const Schema*) {
    if (first_page_id_ == INVALID_PAGE_ID) {
        return End();
    }
    return TableIterator(this, RID(first_page_id_, 0));
}

TableIterator TableHeap::End() {
    return TableIterator(this, RID(INVALID_PAGE_ID, INVALID_SLOT_ID));
}

TableIterator::TableIterator(TableHeap* table_heap, RID rid)
    : table_heap_(table_heap), rid_(rid) {
    if (rid_.IsValid()) {
        Advance();
    }
}

void TableIterator::Advance() {
    while (rid_.GetPageId() != INVALID_PAGE_ID) {
        Page* frame = table_heap_->bpm_->FetchPage(rid_.GetPageId());
        if (frame == nullptr) {
            rid_ = RID(INVALID_PAGE_ID, INVALID_SLOT_ID);
            return;
        }

        SlottedPage page(frame->GetData());
        while (rid_.GetSlotId() < page.GetNumSlots()) {
            std::string_view rec;
            if (page.GetRecord(rid_.GetSlotId(), &rec)) {
                current_tuple_ = Tuple(rec.data(), static_cast<uint32_t>(rec.size()), rid_);
                table_heap_->bpm_->UnpinPage(rid_.GetPageId(), false);
                return;
            }
            rid_ = RID(rid_.GetPageId(), rid_.GetSlotId() + 1);
        }

        // Advance to next page in chain
        page_id_t next_page_id = page.GetNextPageId();
        table_heap_->bpm_->UnpinPage(rid_.GetPageId(), false);
        rid_ = RID(next_page_id, 0);
    }

    rid_ = RID(INVALID_PAGE_ID, INVALID_SLOT_ID);
}

TableIterator& TableIterator::operator++() {
    if (rid_.IsValid()) {
        rid_ = RID(rid_.GetPageId(), rid_.GetSlotId() + 1);
        Advance();
    }
    return *this;
}

} // namespace minidb
