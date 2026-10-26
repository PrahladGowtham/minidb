#pragma once

#include "catalog/catalog.hpp"
#include "parser/parser.hpp"
#include "execution/execution_context.hpp"
#include <string>
#include <vector>

namespace minidb {

struct QueryResult {
    bool success{false};
    std::string message;
    std::vector<std::string> column_names;
    std::vector<std::vector<std::string>> rows;
    int rows_affected{0};
};

class ExecutionEngine {
public:
    ExecutionEngine(Catalog* catalog, BufferPoolManager* bpm);

    QueryResult Execute(const std::string& sql);

private:
    QueryResult ExecuteCreateTable(const CreateTableStatement* stmt);
    QueryResult ExecuteCreateIndex(const CreateIndexStatement* stmt);
    QueryResult ExecuteInsert(const InsertStatement* stmt);
    QueryResult ExecuteSelect(const SelectStatement* stmt);
    QueryResult ExecuteUpdate(const UpdateStatement* stmt);
    QueryResult ExecuteDelete(const DeleteStatement* stmt);

    Catalog* catalog_;
    ExecutionContext exec_ctx_;
};

} // namespace minidb
