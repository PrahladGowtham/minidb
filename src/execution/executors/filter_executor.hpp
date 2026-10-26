#pragma once

#include "execution/executors/abstract_executor.hpp"
#include "parser/ast.hpp"
#include <memory>

namespace minidb {

class FilterExecutor : public AbstractExecutor {
public:
    FilterExecutor(ExecutionContext* exec_ctx, std::unique_ptr<AbstractExecutor> child,
                   AbstractExpression* predicate)
        : AbstractExecutor(exec_ctx), child_(std::move(child)), predicate_(predicate) {}

    void Init() override {
        child_->Init();
    }

    bool Next(Tuple* tuple, RID* rid) override {
        Tuple t;
        RID r;
        while (child_->Next(&t, &r)) {
            if (predicate_ == nullptr) {
                *tuple = t;
                *rid = r;
                return true;
            }

            Value res = predicate_->Evaluate(&t, child_->GetOutputSchema());
            if (!res.IsNull() && res.GetAsBoolean()) {
                *tuple = t;
                *rid = r;
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] const Schema* GetOutputSchema() const override {
        return child_->GetOutputSchema();
    }

private:
    std::unique_ptr<AbstractExecutor> child_;
    AbstractExpression* predicate_;
};

} // namespace minidb
