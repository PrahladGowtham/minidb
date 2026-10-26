#pragma once

#include "catalog/catalog.hpp"
#include "storage/buffer/buffer_pool_manager.hpp"

namespace minidb {

class ExecutionContext {
public:
    ExecutionContext(Catalog* catalog, BufferPoolManager* bpm)
        : catalog_(catalog), bpm_(bpm) {}

    [[nodiscard]] Catalog* GetCatalog() const { return catalog_; }
    [[nodiscard]] BufferPoolManager* GetBufferPoolManager() const { return bpm_; }

private:
    Catalog* catalog_;
    BufferPoolManager* bpm_;
};

} // namespace minidb
