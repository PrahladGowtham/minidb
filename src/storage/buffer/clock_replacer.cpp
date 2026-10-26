#include "storage/buffer/clock_replacer.hpp"

namespace minidb {

ClockReplacer::ClockReplacer(size_t num_frames)
    : num_frames_(num_frames), frames_(num_frames) {}

bool ClockReplacer::Victim(frame_id_t* frame_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (size_ == 0) {
        return false;
    }

    size_t scans = 0;
    while (scans < 2 * num_frames_) {
        ClockFrame& current = frames_[clock_hand_];

        if (current.in_replacer) {
            if (current.ref_bit) {
                // Second chance: clear ref bit and advance
                current.ref_bit = false;
            } else {
                // Found victim
                current.in_replacer = false;
                *frame_id = static_cast<frame_id_t>(clock_hand_);
                size_--;
                clock_hand_ = (clock_hand_ + 1) % num_frames_;
                return true;
            }
        }

        clock_hand_ = (clock_hand_ + 1) % num_frames_;
        scans++;
    }

    return false;
}

void ClockReplacer::Pin(frame_id_t frame_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (frame_id < 0 || static_cast<size_t>(frame_id) >= num_frames_) {
        return;
    }

    ClockFrame& frame = frames_[frame_id];
    if (frame.in_replacer) {
        frame.in_replacer = false;
        frame.ref_bit = false;
        size_--;
    }
}

void ClockReplacer::Unpin(frame_id_t frame_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (frame_id < 0 || static_cast<size_t>(frame_id) >= num_frames_) {
        return;
    }

    ClockFrame& frame = frames_[frame_id];
    if (!frame.in_replacer) {
        frame.in_replacer = true;
        frame.ref_bit = true;
        size_++;
    }
}

size_t ClockReplacer::Size() {
    std::lock_guard<std::mutex> lock(mutex_);
    return size_;
}

} // namespace minidb
