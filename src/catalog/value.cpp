#include "catalog/value.hpp"
#include <sstream>

namespace minidb {

bool Value::GetAsBoolean() const {
    if (is_null_ || type_id_ != TypeId::BOOLEAN) {
        throw DBException("Value is not a valid BOOLEAN");
    }
    return val_bool_;
}

int32_t Value::GetAsInteger() const {
    if (is_null_) throw DBException("Value is NULL");
    if (type_id_ == TypeId::INTEGER) return val_int32_;
    if (type_id_ == TypeId::BIGINT) return static_cast<int32_t>(val_int64_);
    throw DBException("Value is not an INTEGER");
}

int64_t Value::GetAsBigInt() const {
    if (is_null_) throw DBException("Value is NULL");
    if (type_id_ == TypeId::BIGINT) return val_int64_;
    if (type_id_ == TypeId::INTEGER) return static_cast<int64_t>(val_int32_);
    throw DBException("Value is not a BIGINT");
}

float Value::GetAsFloat() const {
    if (is_null_) throw DBException("Value is NULL");
    if (type_id_ == TypeId::FLOAT) return val_float_;
    if (type_id_ == TypeId::DOUBLE) return static_cast<float>(val_double_);
    throw DBException("Value is not a FLOAT");
}

double Value::GetAsDouble() const {
    if (is_null_) throw DBException("Value is NULL");
    if (type_id_ == TypeId::DOUBLE) return val_double_;
    if (type_id_ == TypeId::FLOAT) return static_cast<double>(val_float_);
    if (type_id_ == TypeId::INTEGER) return static_cast<double>(val_int32_);
    if (type_id_ == TypeId::BIGINT) return static_cast<double>(val_int64_);
    throw DBException("Value is not a DOUBLE");
}

const std::string& Value::GetAsVarchar() const {
    if (is_null_ || type_id_ != TypeId::VARCHAR) {
        throw DBException("Value is not a VARCHAR");
    }
    return val_str_;
}

std::string Value::ToString() const {
    if (is_null_) return "NULL";
    switch (type_id_) {
        case TypeId::BOOLEAN: return val_bool_ ? "true" : "false";
        case TypeId::INTEGER: return std::to_string(val_int32_);
        case TypeId::BIGINT:  return std::to_string(val_int64_);
        case TypeId::FLOAT:   return std::to_string(val_float_);
        case TypeId::DOUBLE:  return std::to_string(val_double_);
        case TypeId::VARCHAR: return val_str_;
        default:              return "INVALID";
    }
}

int Value::Compare(const Value& other) const {
    if (is_null_ && other.is_null_) return 0;
    if (is_null_) return -1;
    if (other.is_null_) return 1;

    switch (type_id_) {
        case TypeId::BOOLEAN: {
            bool b1 = val_bool_;
            bool b2 = other.GetAsBoolean();
            if (b1 == b2) return 0;
            return b1 ? 1 : -1;
        }
        case TypeId::INTEGER:
        case TypeId::BIGINT: {
            int64_t v1 = GetAsBigInt();
            int64_t v2 = other.GetAsBigInt();
            if (v1 < v2) return -1;
            if (v1 > v2) return 1;
            return 0;
        }
        case TypeId::FLOAT:
        case TypeId::DOUBLE: {
            double v1 = GetAsDouble();
            double v2 = other.GetAsDouble();
            if (v1 < v2) return -1;
            if (v1 > v2) return 1;
            return 0;
        }
        case TypeId::VARCHAR: {
            const std::string& s1 = val_str_;
            const std::string& s2 = other.GetAsVarchar();
            if (s1 < s2) return -1;
            if (s1 > s2) return 1;
            return 0;
        }
        default:
            return 0;
    }
}

} // namespace minidb
