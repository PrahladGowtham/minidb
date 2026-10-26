#pragma once

#include "common/config.hpp"
#include "common/rid.hpp"
#include "catalog/tuple.hpp"
#include <vector>
#include <cstring>

namespace minidb {

enum class LogRecordType : uint8_t {
    INVALID = 0,
    BEGIN = 1,
    COMMIT = 2,
    ABORT = 3,
    INSERT = 4,
    DELETE = 5,
    UPDATE = 6,
    CLR = 7,
};

/**
 * LogRecord represents a single Write-Ahead Log entry.
 */
class LogRecord {
public:
    #pragma pack(push, 1)
    struct Header {
        uint32_t size{0};
        lsn_t lsn{INVALID_LSN};
        txn_id_t txn_id{INVALID_TXN_ID};
        lsn_t prev_lsn{INVALID_LSN};
        LogRecordType type{LogRecordType::INVALID};
    };
    #pragma pack(pop)

    LogRecord() = default;

    // Txn records (BEGIN, COMMIT, ABORT)
    LogRecord(txn_id_t txn_id, lsn_t prev_lsn, LogRecordType type) {
        header_.size = sizeof(Header);
        header_.txn_id = txn_id;
        header_.prev_lsn = prev_lsn;
        header_.type = type;
    }

    // Insert record
    LogRecord(txn_id_t txn_id, lsn_t prev_lsn, LogRecordType type, const RID& rid, const Tuple& tuple) {
        header_.txn_id = txn_id;
        header_.prev_lsn = prev_lsn;
        header_.type = type;
        rid_ = rid;
        tuple_ = tuple;
        header_.size = sizeof(Header) + sizeof(RID) + sizeof(uint32_t) + tuple_.GetLength();
    }

    [[nodiscard]] const Header& GetHeader() const { return header_; }
    [[nodiscard]] Header& GetHeader() { return header_; }
    [[nodiscard]] lsn_t GetLSN() const { return header_.lsn; }
    void SetLSN(lsn_t lsn) { header_.lsn = lsn; }
    [[nodiscard]] LogRecordType GetType() const { return header_.type; }
    [[nodiscard]] txn_id_t GetTxnId() const { return header_.txn_id; }
    [[nodiscard]] lsn_t GetPrevLSN() const { return header_.prev_lsn; }
    [[nodiscard]] const RID& GetRID() const { return rid_; }
    [[nodiscard]] const Tuple& GetTuple() const { return tuple_; }

    void Serialize(std::vector<char>* buffer) const {
        size_t start = buffer->size();
        buffer->resize(start + header_.size);
        char* ptr = buffer->data() + start;

        std::memcpy(ptr, &header_, sizeof(Header));
        ptr += sizeof(Header);

        if (header_.type == LogRecordType::INSERT || header_.type == LogRecordType::UPDATE) {
            std::memcpy(ptr, &rid_, sizeof(RID));
            ptr += sizeof(RID);

            uint32_t len = tuple_.GetLength();
            std::memcpy(ptr, &len, sizeof(uint32_t));
            ptr += sizeof(uint32_t);

            std::memcpy(ptr, tuple_.GetData(), len);
        }
    }

    static bool Deserialize(const char* data, size_t remaining_bytes, LogRecord* out_record) {
        if (remaining_bytes < sizeof(Header)) return false;

        std::memcpy(&out_record->header_, data, sizeof(Header));
        if (remaining_bytes < out_record->header_.size) return false;

        const char* ptr = data + sizeof(Header);
        if (out_record->header_.type == LogRecordType::INSERT || out_record->header_.type == LogRecordType::UPDATE) {
            std::memcpy(&out_record->rid_, ptr, sizeof(RID));
            ptr += sizeof(RID);

            uint32_t len;
            std::memcpy(&len, ptr, sizeof(uint32_t));
            ptr += sizeof(uint32_t);

            out_record->tuple_ = Tuple(ptr, len, out_record->rid_);
        }
        return true;
    }

private:
    Header header_;
    RID rid_;
    Tuple tuple_;
};

} // namespace minidb
