// SPDX-License-Identifier: MIT
#pragma once

#include "MouseConfig.h"
#include <cstddef>

namespace stopwatch {
struct TouchPoint {
    uint8_t id = 0;
    int x = 0;
    int y = 0;
};

struct TouchFrame {
    bool valid = true;
    uint8_t count = 0;
    TouchPoint points[kMaxContacts]{};
};

// CST8xx: count at 0x02, six-byte contact records starting at 0x03.
// The second record's coordinates end at 0x0c.
inline TouchFrame decodeTouchFrame(const uint8_t* bytes, size_t size) {
    TouchFrame frame;
    if (size < 13 || bytes[2] > kMaxContacts) {
        frame.valid = false;
        return frame;
    }
    for (uint8_t i = 0; i < bytes[2]; ++i) {
        const size_t offset = 3 + i * 6;
        const uint8_t event = bytes[offset] >> 6;
        if (event == 1)
            continue;  // Explicit lift-up record.
        const TouchPoint point{uint8_t(bytes[offset + 2] >> 4),
                               ((bytes[offset] & 0x0f) << 8) | bytes[offset + 1],
                               ((bytes[offset + 2] & 0x0f) << 8) | bytes[offset + 3]};
        if (event == 3 || point.x >= kScreenWidth || point.y >= kScreenHeight ||
            (frame.count && point.id == frame.points[0].id)) {
            frame.valid = false;
            frame.count = 0;
            return frame;
        }
        frame.points[frame.count++] = point;
    }
    return frame;
}
}  // namespace stopwatch
