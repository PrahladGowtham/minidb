#pragma once

#include "common/config.hpp"
#include <string>

namespace minidb {

/**
 * RID (Record Identifier) uniquely identifies a physical record slot within the database.
 * Formatted as: (page_id, slot_id).
 */
class RID {
public:
    RID() : page_id_(INVALID_PAGE_ID), slot_id_(INVALID_SLOT_ID) {}
    RID(page_id_t page_id, slot_id_t slot_id) : page_id_(page_id), slot_id_(slot_id) {}

    [[nodiscard]] page_id_t GetPageId() const { return page_id_; }
    [[nodiscard]] slot_id_t GetSlotId() const { return slot_id_; }

    void Set(page_id_t page_id, slot_id_t slot_id) {
        page_id_ = page_id;
        slot_id_ = slot_id;
    }

    [[nodiscard]] bool IsValid() const {
        return page_id_ != INVALID_PAGE_ID && slot_id_ != INVALID_SLOT_ID;
    }

    bool operator==(const RID& other) const {
        return page_id_ == other.page_id_ && slot_id_ == other.slot_id_;
    }

    bool operator!=(const RID& other) const {
        return !(*this == other);
    }

    [[nodiscard]] std::string ToString() const {
        return "(" + std::to_string(page_id_) + ", " + std::to_string(slot_id_) + ")";
    }

private:
    page_id_t page_id_{INVALID_PAGE_ID};
    slot_id_t slot_id_{INVALID_SLOT_ID};
};

} // namespace minidb
