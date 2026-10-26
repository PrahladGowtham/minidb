#pragma once

#include "catalog/schema.hpp"
#include "catalog/value.hpp"
#include "common/rid.hpp"
#include <vector>
#include <string>

namespace minidb {

/**
 * Tuple represents a physical row conforming to a table Schema.
 */
class Tuple {
public:
    Tuple() = default;
    Tuple(const std::vector<Value>& values, const Schema* schema);
    Tuple(const char* data, uint32_t size, const RID& rid = RID());

    [[nodiscard]] Value GetValue(const Schema* schema, uint32_t col_idx) const;

    [[nodiscard]] const char* GetData() const { return data_.data(); }
    [[nodiscard]] uint32_t GetLength() const { return static_cast<uint32_t>(data_.size()); }

    [[nodiscard]] const RID& GetRID() const { return rid_; }
    void SetRID(const RID& rid) { rid_ = rid; }

    [[nodiscard]] std::string ToString(const Schema* schema) const;

private:
    [[nodiscard]] bool IsNull(uint32_t col_idx) const;
    void SetNull(uint32_t col_idx);

    RID rid_;
    std::vector<char> data_;
};

} // namespace minidb
