#pragma once

#include "common/config.hpp"
#include <cstdint>
#include <string_view>
#include <vector>

namespace minidb {

/**
 * Slotted page layout for storing variable-length records within a 4KB page.
 */
class SlottedPage {
public:
    #pragma pack(push, 1)
    struct Header {
        lsn_t lsn{INVALID_LSN};
        page_id_t page_id{INVALID_PAGE_ID};
        page_id_t prev_page_id{INVALID_PAGE_ID};
        page_id_t next_page_id{INVALID_PAGE_ID};
        uint16_t num_slots{0};
        uint16_t num_records{0};
        uint16_t free_space_pointer{PAGE_SIZE};
        uint16_t flags{0};
    };

    struct Slot {
        uint16_t offset{0};
        uint16_t size{0}; // 0 indicates deleted or unoccupied slot
    };
    #pragma pack(pop)

    static constexpr size_t HEADER_SIZE = sizeof(Header);
    static constexpr size_t SLOT_SIZE = sizeof(Slot);

    explicit SlottedPage(char* page_data) : data_(page_data) {}

    /**
     * Initialize page metadata.
     */
    void Init(page_id_t page_id, page_id_t prev_page_id = INVALID_PAGE_ID, page_id_t next_page_id = INVALID_PAGE_ID);

    /**
     * Insert a record. Returns false if there is not enough free space.
     */
    bool InsertRecord(const char* record_data, uint32_t size, slot_id_t* out_slot_id);

    /**
     * Retrieve record payload by slot_id.
     */
    bool GetRecord(slot_id_t slot_id, std::string_view* out_record) const;

    /**
     * Mark a record as deleted.
     */
    bool DeleteRecord(slot_id_t slot_id);

    /**
     * Update an existing record.
     */
    bool UpdateRecord(slot_id_t slot_id, const char* new_data, uint32_t new_size);

    /**
     * Compact and defragment records on the page to maximize contiguous free space.
     */
    void Defragment();

    /**
     * Calculate contiguous free space remaining without defragmentation.
     */
    [[nodiscard]] size_t GetContiguousFreeSpace() const;

    /**
     * Accessors for header fields.
     */
    [[nodiscard]] Header* GetHeader() { return reinterpret_cast<Header*>(data_); }
    [[nodiscard]] const Header* GetHeader() const { return reinterpret_cast<const Header*>(data_); }

    [[nodiscard]] page_id_t GetPageId() const { return GetHeader()->page_id; }
    [[nodiscard]] page_id_t GetNextPageId() const { return GetHeader()->next_page_id; }
    [[nodiscard]] page_id_t GetPrevPageId() const { return GetHeader()->prev_page_id; }
    void SetNextPageId(page_id_t next_id) { GetHeader()->next_page_id = next_id; }
    void SetPrevPageId(page_id_t prev_id) { GetHeader()->prev_page_id = prev_id; }

    [[nodiscard]] uint16_t GetNumSlots() const { return GetHeader()->num_slots; }
    [[nodiscard]] uint16_t GetNumRecords() const { return GetHeader()->num_records; }

private:
    [[nodiscard]] Slot* GetSlotArray() {
        return reinterpret_cast<Slot*>(data_ + HEADER_SIZE);
    }
    [[nodiscard]] const Slot* GetSlotArray() const {
        return reinterpret_cast<const Slot*>(data_ + HEADER_SIZE);
    }

    char* data_;
};

} // namespace minidb
