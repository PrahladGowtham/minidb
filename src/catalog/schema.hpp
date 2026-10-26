#pragma once

#include "catalog/column.hpp"
#include "common/exception.hpp"
#include <vector>
#include <unordered_map>
#include <string>

namespace minidb {

class Schema {
public:
    Schema() = default;
    explicit Schema(std::vector<Column> columns) : columns_(std::move(columns)) {
        uint32_t curr_offset = 0;
        for (uint32_t i = 0; i < columns_.size(); ++i) {
            columns_[i].SetOffset(curr_offset);
            col_map_[columns_[i].GetName()] = i;
            curr_offset += Column::TypeSize(columns_[i].GetType());
        }
        fixed_size_ = curr_offset;
    }

    [[nodiscard]] const std::vector<Column>& GetColumns() const { return columns_; }
    [[nodiscard]] const Column& GetColumn(uint32_t col_idx) const { return columns_.at(col_idx); }
    [[nodiscard]] uint32_t GetColumnCount() const { return static_cast<uint32_t>(columns_.size()); }
    [[nodiscard]] uint32_t GetFixedSize() const { return fixed_size_; }

    [[nodiscard]] uint32_t GetColIdx(const std::string& col_name) const {
        auto it = col_map_.find(col_name);
        if (it == col_map_.end()) {
            throw DBException("Column not found in schema: " + col_name);
        }
        return it->second;
    }

    [[nodiscard]] bool HasColumn(const std::string& col_name) const {
        return col_map_.find(col_name) != col_map_.end();
    }

private:
    std::vector<Column> columns_;
    std::unordered_map<std::string, uint32_t> col_map_;
    uint32_t fixed_size_{0};
};

} // namespace minidb
