#pragma once

#include <cstdint>
#include <string>

namespace minidb {

enum class TypeId : uint8_t {
    INVALID = 0,
    BOOLEAN = 1,
    INTEGER = 2,
    BIGINT = 3,
    FLOAT = 4,
    DOUBLE = 5,
    VARCHAR = 6,
};

inline std::string TypeIdToString(TypeId type_id) {
    switch (type_id) {
        case TypeId::BOOLEAN: return "BOOLEAN";
        case TypeId::INTEGER: return "INTEGER";
        case TypeId::BIGINT:  return "BIGINT";
        case TypeId::FLOAT:   return "FLOAT";
        case TypeId::DOUBLE:  return "DOUBLE";
        case TypeId::VARCHAR: return "VARCHAR";
        default:              return "INVALID";
    }
}

} // namespace minidb
