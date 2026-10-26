#include "storage/buffer/buffer_pool_manager.hpp"
#include "storage/buffer/clock_replacer.hpp"

namespace minidb {

BufferPoolManager::BufferPoolManager(size_t pool_size, DiskManager* disk_manager)
    : pool_size_(pool_size),
      disk_manager_(disk_manager),
      pages_(new Page[pool_size]),
      replacer_(std::make_unique<ClockReplacer>(pool_size)) {
    for (size_t i = 0; i < pool_size_; ++i) {
        free_list_.push_back(static_cast<frame_id_t>(i));
    }
}

BufferPoolManager::~BufferPoolManager() {
    FlushAllPages();
}

bool BufferPoolManager::FindAvailableFrame(frame_id_t* frame_id) {
    if (!free_list_.empty()) {
        *frame_id = free_list_.front();
        free_list_.pop_front();
        return true;
    }
    return replacer_->Victim(frame_id);
}

Page* BufferPoolManager::FetchPage(page_id_t page_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 1. Check if page already in memory
    auto it = page_table_.find(page_id);
    if (it != page_table_.end()) {
        frame_id_t frame_id = it->second;
        Page* page = &pages_[frame_id];
        page->pin_count_++;
        replacer_->Pin(frame_id);
        return page;
    }

    // 2. Find available frame (free frame or victim)
    frame_id_t frame_id = INVALID_FRAME_ID;
    if (!FindAvailableFrame(&frame_id)) {
        return nullptr; // All frames are currently pinned!
    }

    Page* victim_page = &pages_[frame_id];

    // 3. If victim was dirty, flush it to disk first
    if (victim_page->GetPageId() != INVALID_PAGE_ID) {
        if (victim_page->IsDirty()) {
            disk_manager_->WritePage(victim_page->GetPageId(), victim_page->GetData());
            victim_page->is_dirty_ = false;
        }
        page_table_.erase(victim_page->GetPageId());
    }

    // 4. Read target page from disk into frame
    victim_page->ResetMemory();
    victim_page->page_id_ = page_id;
    victim_page->pin_count_ = 1;
    victim_page->is_dirty_ = false;
    disk_manager_->ReadPage(page_id, victim_page->GetData());

    page_table_[page_id] = frame_id;
    replacer_->Pin(frame_id);

    return victim_page;
}

bool BufferPoolManager::UnpinPage(page_id_t page_id, bool is_dirty) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) {
        return false;
    }

    frame_id_t frame_id = it->second;
    Page* page = &pages_[frame_id];

    if (page->GetPinCount() <= 0) {
        return false;
    }

    if (is_dirty) {
        page->is_dirty_ = true;
    }

    page->pin_count_--;
    if (page->pin_count_ == 0) {
        replacer_->Unpin(frame_id);
    }

    return true;
}

bool BufferPoolManager::FlushPage(page_id_t page_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) {
        return false;
    }

    frame_id_t frame_id = it->second;
    Page* page = &pages_[frame_id];

    if (page->IsDirty()) {
        disk_manager_->WritePage(page->GetPageId(), page->GetData());
        page->is_dirty_ = false;
    }

    return true;
}

Page* BufferPoolManager::NewPage(page_id_t* page_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    frame_id_t frame_id = INVALID_FRAME_ID;
    if (!FindAvailableFrame(&frame_id)) {
        return nullptr;
    }

    Page* victim_page = &pages_[frame_id];

    // Flush victim if dirty
    if (victim_page->GetPageId() != INVALID_PAGE_ID) {
        if (victim_page->IsDirty()) {
            disk_manager_->WritePage(victim_page->GetPageId(), victim_page->GetData());
            victim_page->is_dirty_ = false;
        }
        page_table_.erase(victim_page->GetPageId());
    }

    page_id_t new_page_id = disk_manager_->AllocatePage();

    victim_page->ResetMemory();
    victim_page->page_id_ = new_page_id;
    victim_page->pin_count_ = 1;
    victim_page->is_dirty_ = false;

    page_table_[new_page_id] = frame_id;
    replacer_->Pin(frame_id);

    if (page_id) {
        *page_id = new_page_id;
    }

    return victim_page;
}

bool BufferPoolManager::DeletePage(page_id_t page_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) {
        disk_manager_->DeallocatePage(page_id);
        return true;
    }

    frame_id_t frame_id = it->second;
    Page* page = &pages_[frame_id];

    if (page->GetPinCount() > 0) {
        return false; // Still pinned by an active query
    }

    page_table_.erase(page_id);
    replacer_->Pin(frame_id);
    page->ResetMemory();
    free_list_.push_back(frame_id);

    disk_manager_->DeallocatePage(page_id);
    return true;
}

void BufferPoolManager::FlushAllPages() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& [page_id, frame_id] : page_table_) {
        Page* page = &pages_[frame_id];
        if (page->IsDirty()) {
            disk_manager_->WritePage(page->GetPageId(), page->GetData());
            page->is_dirty_ = false;
        }
    }
}

} // namespace minidb
