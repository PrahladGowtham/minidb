#pragma once

#include "storage/buffer/replacer.hpp"
#include <vector>
#include <mutex>

namespace minidb {

/**
 * ClockReplacer implements the Second-Chance / Clock page replacement policy.
 */
class ClockReplacer : public Replacer {
public:
    explicit ClockReplacer(size_t num_frames);
    ~ClockReplacer() override = default;

    bool Victim(frame_id_t* frame_id) override;
    void Pin(frame_id_t frame_id) override;
    void Unpin(frame_id_t frame_id) override;
    size_t Size() override;

private:
    struct ClockFrame {
        bool in_replacer{false};
        bool ref_bit{false};
    };

    size_t num_frames_;
    size_t clock_hand_{0};
    size_t size_{0};
    std::vector<ClockFrame> frames_;
    std::mutex mutex_;
};

} // namespace minidb
