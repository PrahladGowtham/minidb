#pragma once

#include <cstdint>
#include <cstddef>

namespace minidb {

// Type aliases for database IDs
using page_id_t = int32_t;
using frame_id_t = int32_t;
using lsn_t = int32_t;
using slot_id_t = uint16_t;
using txn_id_t = int64_t;

// Core sizing constants
static constexpr size_t PAGE_SIZE = 4096;
static constexpr size_t DEFAULT_BUFFER_POOL_SIZE = 64;

// Sentinel values
static constexpr page_id_t INVALID_PAGE_ID = -1;
static constexpr frame_id_t INVALID_FRAME_ID = -1;
static constexpr lsn_t INVALID_LSN = -1;
static constexpr txn_id_t INVALID_TXN_ID = -1;
static constexpr slot_id_t INVALID_SLOT_ID = 0xFFFF;

} // namespace minidb
