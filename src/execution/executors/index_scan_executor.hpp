#pragma once

#include "execution/executors/abstract_executor.hpp"

namespace minidb {

class IndexScanExecutor : public AbstractExecutor {
public:
    IndexScanExecutor(ExecutionContext* exec_ctx, TableInfo* table_info, IndexInfo* index_info, int64_t search_key)
        : AbstractExecutor(exec_ctx), table_info_(table_info), index_info_(index_info), search_key_(search_key) {}

    void Init() override {
        cursor_ = 0;
        rids_.clear();
        index_info_->index->GetValue(search_key_, &rids_);
    }

    bool Next(Tuple* tuple, RID* rid) override {
        if (cursor_ >= rids_.size()) {
            return false;
        }

        *rid = rids_[cursor_++];
        return table_info_->table_heap->GetTuple(*rid, tuple);
    }

    [[nodiscard]] const Schema* GetOutputSchema() const override {
        return &table_info_->schema;
    }

private:
    TableInfo* table_info_;
    IndexInfo* index_info_;
    int64_t search_key_;
    std::vector<RID> rids_;
    size_t cursor_{0};
};

} // namespace minidb
