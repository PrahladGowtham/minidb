#include "storage/page/slotted_page.hpp"
#include <cstring>
#include <algorithm>

namespace minidb {

void SlottedPage::Init(page_id_t page_id, page_id_t prev_page_id, page_id_t next_page_id) {
    Header* header = GetHeader();
    header->lsn = INVALID_LSN;
    header->page_id = page_id;
    header->prev_page_id = prev_page_id;
    header->next_page_id = next_page_id;
    header->num_slots = 0;
    header->num_records = 0;
    header->free_space_pointer = static_cast<uint16_t>(PAGE_SIZE);
    header->flags = 0;
}

size_t SlottedPage::GetContiguousFreeSpace() const {
    const Header* header = GetHeader();
    size_t slot_dir_end = HEADER_SIZE + (header->num_slots * SLOT_SIZE);
    if (header->free_space_pointer < slot_dir_end) {
        return 0;
    }
    return header->free_space_pointer - slot_dir_end;
}

bool SlottedPage::InsertRecord(const char* record_data, uint32_t size, slot_id_t* out_slot_id) {
    Header* header = GetHeader();
    Slot* slots = GetSlotArray();

    // Check if there is a reusable tombstone slot
    int reusable_slot_idx = -1;
    for (uint16_t i = 0; i < header->num_slots; ++i) {
        if (slots[i].size == 0) {
            reusable_slot_idx = i;
            break;
        }
    }

    size_t space_needed = size + (reusable_slot_idx != -1 ? 0 : SLOT_SIZE);

    if (GetContiguousFreeSpace() < space_needed) {
        // Try defragmentation in case fragmented deleted records exist
        Defragment();
        if (GetContiguousFreeSpace() < space_needed) {
            return false; // Out of space
        }
    }

    // Allocate record space from bottom of page
    header->free_space_pointer -= static_cast<uint16_t>(size);
    std::memcpy(data_ + header->free_space_pointer, record_data, size);

    slot_id_t target_slot_id = 0;
    if (reusable_slot_idx != -1) {
        target_slot_id = static_cast<slot_id_t>(reusable_slot_idx);
    } else {
        target_slot_id = header->num_slots++;
    }

    slots[target_slot_id].offset = header->free_space_pointer;
    slots[target_slot_id].size = static_cast<uint16_t>(size);
    header->num_records++;

    if (out_slot_id) {
        *out_slot_id = target_slot_id;
    }
    return true;
}

bool SlottedPage::GetRecord(slot_id_t slot_id, std::string_view* out_record) const {
    const Header* header = GetHeader();
    if (slot_id >= header->num_slots) {
        return false;
    }
    const Slot* slots = GetSlotArray();
    if (slots[slot_id].size == 0) {
        return false; // Deleted slot
    }

    if (out_record) {
        *out_record = std::string_view(data_ + slots[slot_id].offset, slots[slot_id].size);
    }
    return true;
}

bool SlottedPage::DeleteRecord(slot_id_t slot_id) {
    Header* header = GetHeader();
    if (slot_id >= header->num_slots) {
        return false;
    }
    Slot* slots = GetSlotArray();
    if (slots[slot_id].size == 0) {
        return false; // Already deleted
    }

    slots[slot_id].size = 0;
    header->num_records--;

    // Trim trailing empty slots if possible
    while (header->num_slots > 0 && slots[header->num_slots - 1].size == 0) {
        header->num_slots--;
    }

    return true;
}

bool SlottedPage::UpdateRecord(slot_id_t slot_id, const char* new_data, uint32_t new_size) {
    Header* header = GetHeader();
    if (slot_id >= header->num_slots) {
        return false;
    }
    Slot* slots = GetSlotArray();
    if (slots[slot_id].size == 0) {
        return false;
    }

    if (new_size <= slots[slot_id].size) {
        // Fits in existing slot offset
        std::memcpy(data_ + slots[slot_id].offset, new_data, new_size);
        slots[slot_id].size = static_cast<uint16_t>(new_size);
        return true;
    }

    // Need more space: delete old slot and reallocate
    slots[slot_id].size = 0;
    header->num_records--;

    if (GetContiguousFreeSpace() < new_size) {
        Defragment();
        if (GetContiguousFreeSpace() < new_size) {
            return false;
        }
    }

    header->free_space_pointer -= static_cast<uint16_t>(new_size);
    std::memcpy(data_ + header->free_space_pointer, new_data, new_size);

    slots[slot_id].offset = header->free_space_pointer;
    slots[slot_id].size = static_cast<uint16_t>(new_size);
    header->num_records++;

    return true;
}

void SlottedPage::Defragment() {
    Header* header = GetHeader();
    Slot* slots = GetSlotArray();

    char temp_buffer[PAGE_SIZE];
    uint16_t new_free_ptr = static_cast<uint16_t>(PAGE_SIZE);

    for (uint16_t i = 0; i < header->num_slots; ++i) {
        if (slots[i].size > 0) {
            new_free_ptr -= slots[i].size;
            std::memcpy(temp_buffer + new_free_ptr, data_ + slots[i].offset, slots[i].size);
            slots[i].offset = new_free_ptr;
        }
    }

    // Copy compacted records back
    size_t bytes_to_copy = PAGE_SIZE - new_free_ptr;
    if (bytes_to_copy > 0) {
        std::memcpy(data_ + new_free_ptr, temp_buffer + new_free_ptr, bytes_to_copy);
    }
    header->free_space_pointer = new_free_ptr;
}

} // namespace minidb
