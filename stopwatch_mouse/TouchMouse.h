// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#include <cstdlib>

// Transport-independent relative pointer. Lifting the finger never moves it.
class TouchMouse {
   public:
    float sensitivity = 0.65f;
    void reset() {
        tracking = false;
        remainderX = remainderY = 0;
    }
    bool update(bool down, int x, int y, int8_t& dx, int8_t& dy) {
        dx = dy = 0;
        if (!down) {
            reset();
            return false;
        }
        if (!tracking) {
            tracking = true;
            lastX = x;
            lastY = y;
            return false;
        }
        int deltaX = x - lastX, deltaY = y - lastY;
        lastX = x;
        lastY = y;
        // Ignore contact jumps; do not launch the pointer across the desktop.
        if (std::abs(deltaX) > kMaxContactStep || std::abs(deltaY) > kMaxContactStep) {
            remainderX = remainderY = 0;
            return false;
        }
        remainderX += deltaX * sensitivity;
        remainderY += deltaY * sensitivity;
        int ix = (int)remainderX, iy = (int)remainderY;
        dx = static_cast<int8_t>(ix < -127 ? -127 : ix > 127 ? 127 : ix);
        dy = static_cast<int8_t>(iy < -127 ? -127 : iy > 127 ? 127 : iy);
        remainderX -= dx;
        remainderY -= dy;
        return dx || dy;
    }

   private:
    static constexpr int kMaxContactStep = 100;
    bool tracking = false;
    int lastX = 0, lastY = 0;
    float remainderX = 0, remainderY = 0;
};
