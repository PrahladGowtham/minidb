#pragma once

#include "catalog/schema.hpp"
#include "catalog/tuple.hpp"
#include "common/rid.hpp"
#include "execution/execution_context.hpp"

namespace minidb {

/**
 * AbstractExecutor implements the classic Volcano iterator model.
 */
class AbstractExecutor {
public:
    explicit AbstractExecutor(ExecutionContext* exec_ctx) : exec_ctx_(exec_ctx) {}
    virtual ~AbstractExecutor() = default;

    virtual void Init() = 0;
    virtual bool Next(Tuple* tuple, RID* rid) = 0;
    [[nodiscard]] virtual const Schema* GetOutputSchema() const = 0;

    [[nodiscard]] ExecutionContext* GetExecutionContext() const { return exec_ctx_; }

private:
    ExecutionContext* exec_ctx_;
};

} // namespace minidb
