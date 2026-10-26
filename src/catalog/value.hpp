#pragma once

#include "catalog/type.hpp"
#include "common/exception.hpp"
#include <string>
#include <string_view>
#include <variant>

namespace minidb {

/**
 * Value encapsulates a dynamically-typed SQL scalar value with nullability.
 */
class Value {
public:
    Value() : type_id_(TypeId::INVALID), is_null_(true) {}
    explicit Value(TypeId type) : type_id_(type), is_null_(true) {}
    explicit Value(bool val) : type_id_(TypeId::BOOLEAN), is_null_(false), val_bool_(val) {}
    explicit Value(int32_t val) : type_id_(TypeId::INTEGER), is_null_(false), val_int32_(val) {}
    explicit Value(int64_t val) : type_id_(TypeId::BIGINT), is_null_(false), val_int64_(val) {}
    explicit Value(float val) : type_id_(TypeId::FLOAT), is_null_(false), val_float_(val) {}
    explicit Value(double val) : type_id_(TypeId::DOUBLE), is_null_(false), val_double_(val) {}
    explicit Value(std::string val) : type_id_(TypeId::VARCHAR), is_null_(false), val_str_(std::move(val)) {}
    explicit Value(std::string_view val) : type_id_(TypeId::VARCHAR), is_null_(false), val_str_(val) {}

    [[nodiscard]] TypeId GetTypeId() const { return type_id_; }
    [[nodiscard]] bool IsNull() const { return is_null_; }

    [[nodiscard]] bool GetAsBoolean() const;
    [[nodiscard]] int32_t GetAsInteger() const;
    [[nodiscard]] int64_t GetAsBigInt() const;
    [[nodiscard]] float GetAsFloat() const;
    [[nodiscard]] double GetAsDouble() const;
    [[nodiscard]] const std::string& GetAsVarchar() const;

    [[nodiscard]] std::string ToString() const;

    // Comparisons
    [[nodiscard]] int Compare(const Value& other) const;
    bool operator==(const Value& other) const { return Compare(other) == 0; }
    bool operator!=(const Value& other) const { return Compare(other) != 0; }
    bool operator<(const Value& other) const { return Compare(other) < 0; }
    bool operator<=(const Value& other) const { return Compare(other) <= 0; }
    bool operator>(const Value& other) const { return Compare(other) > 0; }
    bool operator>=(const Value& other) const { return Compare(other) >= 0; }

private:
    TypeId type_id_{TypeId::INVALID};
    bool is_null_{true};

    bool val_bool_{false};
    int32_t val_int32_{0};
    int64_t val_int64_{0};
    float val_float_{0.0f};
    double val_double_{0.0};
    std::string val_str_;
};

} // namespace minidb
