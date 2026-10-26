#pragma once

#include "common/config.hpp"

namespace minidb {

/**
 * Replacer is an abstract base class for tracking unpinned frames in the buffer pool.
 */
class Replacer {
public:
    Replacer() = default;
    virtual ~Replacer() = default;

    /**
     * Remove the victim frame as decided by the replacement policy.
     * Returns true if a victim was found, false otherwise.
     */
    virtual bool Victim(frame_id_t* frame_id) = 0;

    /**
     * Pins a frame, indicating that it should not be victimized until it is unpinned.
     */
    virtual void Pin(frame_id_t frame_id) = 0;

    /**
     * Unpins a frame, indicating that it can now be victimized.
     */
    virtual void Unpin(frame_id_t frame_id) = 0;

    /**
     * Returns the number of frames that are currently eligible for eviction.
     */
    virtual size_t Size() = 0;
};

} // namespace minidb
