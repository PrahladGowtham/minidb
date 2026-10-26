#include "execution/execution_engine.hpp"
#include "execution/executors/seq_scan_executor.hpp"
#include "execution/executors/index_scan_executor.hpp"
#include "execution/executors/filter_executor.hpp"
#include "common/exception.hpp"

#include <iostream>

namespace minidb {

ExecutionEngine::ExecutionEngine(Catalog* catalog, BufferPoolManager* bpm)
    : catalog_(catalog), exec_ctx_(catalog, bpm) {}

QueryResult ExecutionEngine::Execute(const std::string& sql) {
    try {
        Parser parser(sql);
        auto stmt = parser.ParseStatement();
        if (!stmt) {
            return QueryResult{.success = false, .message = "Empty statement"};
        }

        switch (stmt->GetType()) {
            case StatementType::CREATE_TABLE:
                return ExecuteCreateTable(dynamic_cast<CreateTableStatement*>(stmt.get()));
            case StatementType::CREATE_INDEX:
                return ExecuteCreateIndex(dynamic_cast<CreateIndexStatement*>(stmt.get()));
            case StatementType::INSERT:
                return ExecuteInsert(dynamic_cast<InsertStatement*>(stmt.get()));
            case StatementType::SELECT:
                return ExecuteSelect(dynamic_cast<SelectStatement*>(stmt.get()));
            case StatementType::UPDATE:
                return ExecuteUpdate(dynamic_cast<UpdateStatement*>(stmt.get()));
            case StatementType::DELETE:
                return ExecuteDelete(dynamic_cast<DeleteStatement*>(stmt.get()));
            default:
                return QueryResult{.success = false, .message = "Unsupported statement type"};
        }
    } catch (const DBException& e) {
        return QueryResult{.success = false, .message = e.what()};
    } catch (const std::exception& e) {
        return QueryResult{.success = false, .message = std::string("Internal error: ") + e.what()};
    }
}

QueryResult ExecutionEngine::ExecuteCreateTable(const CreateTableStatement* stmt) {
    Schema schema(stmt->GetColumns());
    catalog_->CreateTable(stmt->GetTableName(), schema);
    return QueryResult{
        .success = true,
        .message = "Table '" + stmt->GetTableName() + "' created successfully.",
        .rows_affected = 0
    };
}

QueryResult ExecutionEngine::ExecuteCreateIndex(const CreateIndexStatement* stmt) {
    TableInfo* table = catalog_->GetTable(stmt->GetTableName());
    if (table == nullptr) {
        throw DBException("Table not found: " + stmt->GetTableName());
    }

    uint32_t col_idx = table->schema.GetColIdx(stmt->GetColumnName());
    IndexInfo* idx = catalog_->CreateIndex(stmt->GetIndexName(), stmt->GetTableName(), {col_idx});

    // Populate existing records into index
    int count = 0;
    for (auto it = table->table_heap->Begin(&table->schema); it != table->table_heap->End(); ++it) {
        Value v = it->GetValue(&table->schema, col_idx);
        idx->index->Insert(v.GetAsBigInt(), it->GetRID());
        count++;
    }

    return QueryResult{
        .success = true,
        .message = "Index '" + stmt->GetIndexName() + "' created on column '" + stmt->GetColumnName() +
                   "' (" + std::to_string(count) + " keys indexed).",
        .rows_affected = count
    };
}

QueryResult ExecutionEngine::ExecuteInsert(const InsertStatement* stmt) {
    TableInfo* table = catalog_->GetTable(stmt->GetTableName());
    if (table == nullptr) {
        throw DBException("Table not found: " + stmt->GetTableName());
    }

    auto indexes = catalog_->GetTableIndexes(stmt->GetTableName());
    int inserted_count = 0;

    for (const auto& row_values : stmt->GetValues()) {
        if (row_values.size() != table->schema.GetColumnCount()) {
            throw DBException("Column count mismatch in INSERT statement");
        }

        Tuple tuple(row_values, &table->schema);
        RID rid;
        if (!table->table_heap->InsertTuple(tuple, &rid)) {
            throw DBException("Failed to insert tuple into table heap");
        }

        // Update all secondary indexes
        for (auto* idx : indexes) {
            uint32_t indexed_col = idx->key_attrs[0];
            Value v = tuple.GetValue(&table->schema, indexed_col);
            idx->index->Insert(v.GetAsBigInt(), rid);
        }

        inserted_count++;
    }

    return QueryResult{
        .success = true,
        .message = "Inserted " + std::to_string(inserted_count) + " row(s).",
        .rows_affected = inserted_count
    };
}

QueryResult ExecutionEngine::ExecuteSelect(const SelectStatement* stmt) {
    TableInfo* table = catalog_->GetTable(stmt->GetTableName());
    if (table == nullptr) {
        throw DBException("Table not found: " + stmt->GetTableName());
    }

    // Check for Index Scan optimization
    std::unique_ptr<AbstractExecutor> root_executor = nullptr;
    bool used_index = false;

    if (stmt->HasWhereClause()) {
        auto* where_expr = stmt->GetWhereClause();
        if (where_expr->GetExpressionType() == ExpressionType::COMPARISON) {
            auto* comp = dynamic_cast<ComparisonExpression*>(where_expr);
            if (comp->GetComparisonType() == ComparisonType::EQUAL &&
                comp->GetLeft()->GetExpressionType() == ExpressionType::COLUMN_REF &&
                comp->GetRight()->GetExpressionType() == ExpressionType::CONSTANT) {

                auto* col_ref = dynamic_cast<ColumnRefExpression*>(comp->GetLeft());
                auto* const_expr = dynamic_cast<ConstantExpression*>(comp->GetRight());

                auto indexes = catalog_->GetTableIndexes(stmt->GetTableName());
                for (auto* idx : indexes) {
                    uint32_t indexed_col = idx->key_attrs[0];
                    if (table->schema.GetColumn(indexed_col).GetName() == col_ref->GetColumnName()) {
                        int64_t key = const_expr->GetValue().GetAsBigInt();
                        root_executor = std::make_unique<IndexScanExecutor>(&exec_ctx_, table, idx, key);
                        used_index = true;
                        break;
                    }
                }
            }
        }
    }

    if (!used_index) {
        auto seq_scan = std::make_unique<SeqScanExecutor>(&exec_ctx_, table);
        if (stmt->HasWhereClause()) {
            root_executor = std::make_unique<FilterExecutor>(&exec_ctx_, std::move(seq_scan), stmt->GetWhereClause());
        } else {
            root_executor = std::move(seq_scan);
        }
    }

    root_executor->Init();

    // Determine output columns
    std::vector<uint32_t> proj_indices;
    std::vector<std::string> proj_names;

    if (stmt->GetSelectCols().size() == 1 && stmt->GetSelectCols()[0] == "*") {
        for (uint32_t i = 0; i < table->schema.GetColumnCount(); ++i) {
            proj_indices.push_back(i);
            proj_names.push_back(table->schema.GetColumn(i).GetName());
        }
    } else {
        for (const auto& col_name : stmt->GetSelectCols()) {
            proj_indices.push_back(table->schema.GetColIdx(col_name));
            proj_names.push_back(col_name);
        }
    }

    std::vector<std::vector<std::string>> output_rows;
    Tuple tuple;
    RID rid;

    while (root_executor->Next(&tuple, &rid)) {
        std::vector<std::string> row;
        row.reserve(proj_indices.size());
        for (uint32_t col_idx : proj_indices) {
            row.push_back(tuple.GetValue(&table->schema, col_idx).ToString());
        }
        output_rows.push_back(std::move(row));
    }

    return QueryResult{
        .success = true,
        .message = (used_index ? "[Index Scan] " : "[Sequential Scan] ") +
                   std::to_string(output_rows.size()) + " row(s) returned.",
        .column_names = std::move(proj_names),
        .rows = std::move(output_rows),
        .rows_affected = static_cast<int>(output_rows.size())
    };
}

QueryResult ExecutionEngine::ExecuteUpdate(const UpdateStatement* stmt) {
    TableInfo* table = catalog_->GetTable(stmt->GetTableName());
    if (table == nullptr) {
        throw DBException("Table not found: " + stmt->GetTableName());
    }

    uint32_t target_col_idx = table->schema.GetColIdx(stmt->GetColumnName());
    auto seq_scan = std::make_unique<SeqScanExecutor>(&exec_ctx_, table);
    auto filter = std::make_unique<FilterExecutor>(&exec_ctx_, std::move(seq_scan), stmt->GetWhereClause());
    filter->Init();

    std::vector<RID> target_rids;
    Tuple tuple;
    RID rid;
    while (filter->Next(&tuple, &rid)) {
        target_rids.push_back(rid);
    }

    for (const auto& r : target_rids) {
        Tuple old_tuple;
        table->table_heap->GetTuple(r, &old_tuple);

        std::vector<Value> values;
        for (uint32_t i = 0; i < table->schema.GetColumnCount(); ++i) {
            if (i == target_col_idx) {
                values.push_back(stmt->GetNewValue());
            } else {
                values.push_back(old_tuple.GetValue(&table->schema, i));
            }
        }
        Tuple updated_tuple(values, &table->schema);
        table->table_heap->UpdateTuple(updated_tuple, r);
    }

    return QueryResult{
        .success = true,
        .message = "Updated " + std::to_string(target_rids.size()) + " row(s).",
        .rows_affected = static_cast<int>(target_rids.size())
    };
}

QueryResult ExecutionEngine::ExecuteDelete(const DeleteStatement* stmt) {
    TableInfo* table = catalog_->GetTable(stmt->GetTableName());
    if (table == nullptr) {
        throw DBException("Table not found: " + stmt->GetTableName());
    }

    auto seq_scan = std::make_unique<SeqScanExecutor>(&exec_ctx_, table);
    auto filter = std::make_unique<FilterExecutor>(&exec_ctx_, std::move(seq_scan), stmt->GetWhereClause());
    filter->Init();

    std::vector<RID> target_rids;
    Tuple tuple;
    RID rid;
    while (filter->Next(&tuple, &rid)) {
        target_rids.push_back(rid);
    }

    for (const auto& r : target_rids) {
        table->table_heap->DeleteTuple(r);
    }

    return QueryResult{
        .success = true,
        .message = "Deleted " + std::to_string(target_rids.size()) + " row(s).",
        .rows_affected = static_cast<int>(target_rids.size())
    };
}

} // namespace minidb
