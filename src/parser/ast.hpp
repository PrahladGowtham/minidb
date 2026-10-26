#pragma once

#include "catalog/schema.hpp"
#include "catalog/tuple.hpp"
#include "catalog/value.hpp"
#include <memory>
#include <string>
#include <vector>

namespace minidb {

enum class ExpressionType {
    CONSTANT,
    COLUMN_REF,
    COMPARISON,
};

enum class ComparisonType {
    EQUAL,
    NOT_EQUAL,
    LESS_THAN,
    LESS_EQUAL,
    GREATER_THAN,
    GREATER_EQUAL,
};

class AbstractExpression {
public:
    virtual ~AbstractExpression() = default;
    [[nodiscard]] virtual ExpressionType GetExpressionType() const = 0;
    [[nodiscard]] virtual Value Evaluate(const Tuple* tuple, const Schema* schema) const = 0;
};

class ConstantExpression : public AbstractExpression {
public:
    explicit ConstantExpression(Value val) : val_(std::move(val)) {}
    [[nodiscard]] ExpressionType GetExpressionType() const override { return ExpressionType::CONSTANT; }
    [[nodiscard]] Value Evaluate(const Tuple*, const Schema*) const override { return val_; }
    [[nodiscard]] const Value& GetValue() const { return val_; }

private:
    Value val_;
};

class ColumnRefExpression : public AbstractExpression {
public:
    explicit ColumnRefExpression(std::string col_name) : col_name_(std::move(col_name)) {}
    [[nodiscard]] ExpressionType GetExpressionType() const override { return ExpressionType::COLUMN_REF; }

    [[nodiscard]] Value Evaluate(const Tuple* tuple, const Schema* schema) const override {
        uint32_t col_idx = schema->GetColIdx(col_name_);
        return tuple->GetValue(schema, col_idx);
    }

    [[nodiscard]] const std::string& GetColumnName() const { return col_name_; }

private:
    std::string col_name_;
};

class ComparisonExpression : public AbstractExpression {
public:
    ComparisonExpression(std::unique_ptr<AbstractExpression> left,
                         std::unique_ptr<AbstractExpression> right,
                         ComparisonType comp_type)
        : left_(std::move(left)), right_(std::move(right)), comp_type_(comp_type) {}

    [[nodiscard]] ExpressionType GetExpressionType() const override { return ExpressionType::COMPARISON; }

    [[nodiscard]] Value Evaluate(const Tuple* tuple, const Schema* schema) const override {
        Value l = left_->Evaluate(tuple, schema);
        Value r = right_->Evaluate(tuple, schema);
        int res = l.Compare(r);

        switch (comp_type_) {
            case ComparisonType::EQUAL:         return Value(res == 0);
            case ComparisonType::NOT_EQUAL:     return Value(res != 0);
            case ComparisonType::LESS_THAN:     return Value(res < 0);
            case ComparisonType::LESS_EQUAL:    return Value(res <= 0);
            case ComparisonType::GREATER_THAN:  return Value(res > 0);
            case ComparisonType::GREATER_EQUAL: return Value(res >= 0);
        }
        return Value(false);
    }

    [[nodiscard]] AbstractExpression* GetLeft() const { return left_.get(); }
    [[nodiscard]] AbstractExpression* GetRight() const { return right_.get(); }
    [[nodiscard]] ComparisonType GetComparisonType() const { return comp_type_; }

private:
    std::unique_ptr<AbstractExpression> left_;
    std::unique_ptr<AbstractExpression> right_;
    ComparisonType comp_type_;
};

// Statements
enum class StatementType {
    CREATE_TABLE,
    CREATE_INDEX,
    INSERT,
    SELECT,
    UPDATE,
    DELETE,
    EXPLAIN,
};

class SQLStatement {
public:
    virtual ~SQLStatement() = default;
    [[nodiscard]] virtual StatementType GetType() const = 0;
};

class CreateTableStatement : public SQLStatement {
public:
    CreateTableStatement(std::string table_name, std::vector<Column> columns)
        : table_name_(std::move(table_name)), columns_(std::move(columns)) {}

    [[nodiscard]] StatementType GetType() const override { return StatementType::CREATE_TABLE; }
    [[nodiscard]] const std::string& GetTableName() const { return table_name_; }
    [[nodiscard]] const std::vector<Column>& GetColumns() const { return columns_; }

private:
    std::string table_name_;
    std::vector<Column> columns_;
};

class CreateIndexStatement : public SQLStatement {
public:
    CreateIndexStatement(std::string index_name, std::string table_name, std::string col_name)
        : index_name_(std::move(index_name)), table_name_(std::move(table_name)), col_name_(std::move(col_name)) {}

    [[nodiscard]] StatementType GetType() const override { return StatementType::CREATE_INDEX; }
    [[nodiscard]] const std::string& GetIndexName() const { return index_name_; }
    [[nodiscard]] const std::string& GetTableName() const { return table_name_; }
    [[nodiscard]] const std::string& GetColumnName() const { return col_name_; }

private:
    std::string index_name_;
    std::string table_name_;
    std::string col_name_;
};

class InsertStatement : public SQLStatement {
public:
    InsertStatement(std::string table_name, std::vector<std::vector<Value>> values)
        : table_name_(std::move(table_name)), values_(std::move(values)) {}

    [[nodiscard]] StatementType GetType() const override { return StatementType::INSERT; }
    [[nodiscard]] const std::string& GetTableName() const { return table_name_; }
    [[nodiscard]] const std::vector<std::vector<Value>>& GetValues() const { return values_; }

private:
    std::string table_name_;
    std::vector<std::vector<Value>> values_;
};

class SelectStatement : public SQLStatement {
public:
    SelectStatement(std::string table_name, std::vector<std::string> select_cols,
                    std::unique_ptr<AbstractExpression> where_clause = nullptr)
        : table_name_(std::move(table_name)),
          select_cols_(std::move(select_cols)),
          where_clause_(std::move(where_clause)) {}

    [[nodiscard]] StatementType GetType() const override { return StatementType::SELECT; }
    [[nodiscard]] const std::string& GetTableName() const { return table_name_; }
    [[nodiscard]] const std::vector<std::string>& GetSelectCols() const { return select_cols_; }
    [[nodiscard]] AbstractExpression* GetWhereClause() const { return where_clause_.get(); }
    [[nodiscard]] bool HasWhereClause() const { return where_clause_ != nullptr; }

private:
    std::string table_name_;
    std::vector<std::string> select_cols_;
    std::unique_ptr<AbstractExpression> where_clause_;
};

class UpdateStatement : public SQLStatement {
public:
    UpdateStatement(std::string table_name, std::string col_name, Value new_value,
                    std::unique_ptr<AbstractExpression> where_clause)
        : table_name_(std::move(table_name)),
          col_name_(std::move(col_name)),
          new_value_(std::move(new_value)),
          where_clause_(std::move(where_clause)) {}

    [[nodiscard]] StatementType GetType() const override { return StatementType::UPDATE; }
    [[nodiscard]] const std::string& GetTableName() const { return table_name_; }
    [[nodiscard]] const std::string& GetColumnName() const { return col_name_; }
    [[nodiscard]] const Value& GetNewValue() const { return new_value_; }
    [[nodiscard]] AbstractExpression* GetWhereClause() const { return where_clause_.get(); }

private:
    std::string table_name_;
    std::string col_name_;
    Value new_value_;
    std::unique_ptr<AbstractExpression> where_clause_;
};

class DeleteStatement : public SQLStatement {
public:
    DeleteStatement(std::string table_name, std::unique_ptr<AbstractExpression> where_clause)
        : table_name_(std::move(table_name)), where_clause_(std::move(where_clause)) {}

    [[nodiscard]] StatementType GetType() const override { return StatementType::DELETE; }
    [[nodiscard]] const std::string& GetTableName() const { return table_name_; }
    [[nodiscard]] AbstractExpression* GetWhereClause() const { return where_clause_.get(); }

private:
    std::string table_name_;
    std::unique_ptr<AbstractExpression> where_clause_;
};

} // namespace minidb
