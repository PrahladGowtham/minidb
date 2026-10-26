#pragma once

#include "execution/executors/abstract_executor.hpp"

namespace minidb {

class SeqScanExecutor : public AbstractExecutor {
public:
    SeqScanExecutor(ExecutionContext* exec_ctx, TableInfo* table_info)
        : AbstractExecutor(exec_ctx), table_info_(table_info) {}

    void Init() override {
        iter_ = table_info_->table_heap->Begin(&table_info_->schema);
    }

    bool Next(Tuple* tuple, RID* rid) override {
        if (iter_ == table_info_->table_heap->End()) {
            return false;
        }

        *tuple = *iter_;
        *rid = tuple->GetRID();
        ++iter_;
        return true;
    }

    [[nodiscard]] const Schema* GetOutputSchema() const override {
        return &table_info_->schema;
    }

private:
    TableInfo* table_info_;
    TableIterator iter_;
};

} // namespace minidb
