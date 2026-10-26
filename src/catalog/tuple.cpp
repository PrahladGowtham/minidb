#include "catalog/tuple.hpp"
#include <cstring>
#include <sstream>

namespace minidb {

Tuple::Tuple(const std::vector<Value>& values, const Schema* schema) {
    uint32_t col_count = schema->GetColumnCount();
    uint32_t null_bitmap_size = (col_count + 7) / 8;
    uint32_t fixed_data_size = schema->GetFixedSize();

    // Calculate variable data size
    uint32_t var_data_size = 0;
    for (uint32_t i = 0; i < col_count; ++i) {
        if (!values[i].IsNull() && schema->GetColumn(i).GetType() == TypeId::VARCHAR) {
            var_data_size += static_cast<uint32_t>(values[i].GetAsVarchar().size());
        }
    }

    uint32_t total_size = null_bitmap_size + fixed_data_size + var_data_size;
    data_.resize(total_size, 0);

    char* null_bitmap = data_.data();
    char* fixed_area = data_.data() + null_bitmap_size;
    char* var_pool = fixed_area + fixed_data_size;
    uint32_t var_offset = 0;

    for (uint32_t i = 0; i < col_count; ++i) {
        const auto& col = schema->GetColumn(i);
        const auto& val = values[i];

        if (val.IsNull()) {
            null_bitmap[i / 8] |= (1 << (i % 8));
            continue;
        }

        char* dest = fixed_area + col.GetOffset();
        switch (col.GetType()) {
            case TypeId::BOOLEAN: {
                bool b = val.GetAsBoolean();
                std::memcpy(dest, &b, 1);
                break;
            }
            case TypeId::INTEGER: {
                int32_t v = val.GetAsInteger();
                std::memcpy(dest, &v, 4);
                break;
            }
            case TypeId::BIGINT: {
                int64_t v = val.GetAsBigInt();
                std::memcpy(dest, &v, 8);
                break;
            }
            case TypeId::FLOAT: {
                float v = val.GetAsFloat();
                std::memcpy(dest, &v, 4);
                break;
            }
            case TypeId::DOUBLE: {
                double v = val.GetAsDouble();
                std::memcpy(dest, &v, 8);
                break;
            }
            case TypeId::VARCHAR: {
                const std::string& str = val.GetAsVarchar();
                uint16_t str_len = static_cast<uint16_t>(str.size());
                uint16_t str_offset = static_cast<uint16_t>(var_offset);

                std::memcpy(dest, &str_offset, 2);
                std::memcpy(dest + 2, &str_len, 2);

                std::memcpy(var_pool + var_offset, str.data(), str_len);
                var_offset += str_len;
                break;
            }
            default:
                break;
        }
    }
}

Tuple::Tuple(const char* data, uint32_t size, const RID& rid) : rid_(rid) {
    data_.assign(data, data + size);
}

bool Tuple::IsNull(uint32_t col_idx) const {
    const char* null_bitmap = data_.data();
    return (null_bitmap[col_idx / 8] & (1 << (col_idx % 8))) != 0;
}

Value Tuple::GetValue(const Schema* schema, uint32_t col_idx) const {
    if (IsNull(col_idx)) {
        return Value(schema->GetColumn(col_idx).GetType());
    }

    uint32_t col_count = schema->GetColumnCount();
    uint32_t null_bitmap_size = (col_count + 7) / 8;
    const char* fixed_area = data_.data() + null_bitmap_size;
    const char* var_pool = fixed_area + schema->GetFixedSize();

    const auto& col = schema->GetColumn(col_idx);
    const char* src = fixed_area + col.GetOffset();

    switch (col.GetType()) {
        case TypeId::BOOLEAN: {
            bool b;
            std::memcpy(&b, src, 1);
            return Value(b);
        }
        case TypeId::INTEGER: {
            int32_t v;
            std::memcpy(&v, src, 4);
            return Value(v);
        }
        case TypeId::BIGINT: {
            int64_t v;
            std::memcpy(&v, src, 8);
            return Value(v);
        }
        case TypeId::FLOAT: {
            float v;
            std::memcpy(&v, src, 4);
            return Value(v);
        }
        case TypeId::DOUBLE: {
            double v;
            std::memcpy(&v, src, 8);
            return Value(v);
        }
        case TypeId::VARCHAR: {
            uint16_t str_offset;
            uint16_t str_len;
            std::memcpy(&str_offset, src, 2);
            std::memcpy(&str_len, src + 2, 2);
            return Value(std::string_view(var_pool + str_offset, str_len));
        }
        default:
            return Value();
    }
}

std::string Tuple::ToString(const Schema* schema) const {
    std::ostringstream ss;
    ss << "(";
    for (uint32_t i = 0; i < schema->GetColumnCount(); ++i) {
        if (i > 0) ss << ", ";
        ss << GetValue(schema, i).ToString();
    }
    ss << ")";
    return ss.str();
}

} // namespace minidb
