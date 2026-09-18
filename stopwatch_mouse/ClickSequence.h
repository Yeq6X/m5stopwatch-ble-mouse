// SPDX-License-Identifier: MIT
#pragma once

#include "MouseConfig.h"

namespace stopwatch {
// Nonblocking HID down/up pairs with a release interval between double-clicks.
class ClickSequence {
   public:
    void reset() {
        button_ = remaining_ = 0;
        down_ = false;
    }

    void start(uint8_t button, uint8_t count, uint32_t now) {
        button_ = button;
        remaining_ = count;
        down_ = count != 0;
        changed_ = now;
    }

    uint8_t update(uint32_t now) {
        if (remaining_ && uint32_t(now - changed_) >= kClickPulseMs) {
            // Advance one edge per sample so a delayed loop never skips a release.
            changed_ = now;
            if (down_) {
                down_ = false;
                --remaining_;
            } else {
                down_ = true;
            }
        }
        return down_ ? button_ : 0;
    }

   private:
    uint8_t button_ = 0, remaining_ = 0;
    bool down_ = false;
    uint32_t changed_ = 0;
};
}  // namespace stopwatch
