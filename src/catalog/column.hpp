#pragma once

#include "catalog/type.hpp"
#include <string>
#include <utility>

namespace minidb {

class Column {
public:
    Column(std::string name, TypeId type, bool nullable = true, uint32_t length = 0)
        : name_(std::move(name)),
          type_(type),
          nullable_(nullable),
          length_(type == TypeId::VARCHAR ? length : TypeSize(type)) {}

    [[nodiscard]] const std::string& GetName() const { return name_; }
    [[nodiscard]] TypeId GetType() const { return type_; }
    [[nodiscard]] bool IsNullable() const { return nullable_; }
    [[nodiscard]] uint32_t GetLength() const { return length_; }
    [[nodiscard]] uint32_t GetOffset() const { return offset_; }
    void SetOffset(uint32_t offset) { offset_ = offset; }

    [[nodiscard]] bool IsInlined() const { return type_ != TypeId::VARCHAR; }

    static uint32_t TypeSize(TypeId type) {
        switch (type) {
            case TypeId::BOOLEAN: return 1;
            case TypeId::INTEGER: return 4;
            case TypeId::BIGINT:  return 8;
            case TypeId::FLOAT:   return 4;
            case TypeId::DOUBLE:  return 8;
            case TypeId::VARCHAR: return 4; // Stored as (uint16_t offset, uint16_t length) in fixed part
            default:              return 0;
        }
    }

private:
    std::string name_;
    TypeId type_{TypeId::INVALID};
    bool nullable_{true};
    uint32_t length_{0};
    uint32_t offset_{0};
};

} // namespace minidb
