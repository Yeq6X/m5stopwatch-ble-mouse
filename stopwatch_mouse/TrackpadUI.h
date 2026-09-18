// SPDX-License-Identifier: MIT
#pragma once
#include "StopWatchHardware.h"
#include <math.h>

namespace stopwatch {
class TrackpadUI {
    StopWatchHardware& hardware;
    M5Canvas background, frame;
    bool ready = false, wasTouch = false;
    uint8_t oldButtons = 255;
    int oldX = 0, oldY = 0;
    uint32_t lastFrame = 0;
    static constexpr uint16_t black = 0x0000, muted = 0x7BEF, cyan = 0x5EFB;

   public:
    explicit TrackpadUI(StopWatchHardware& h) : hardware(h) {}
    bool begin() {
        background.setColorDepth(16);
        frame.setColorDepth(16);
        background.setPsram(true);
        frame.setPsram(true);
        return ready = background.createSprite(kScreenWidth, kScreenHeight) &&
                       frame.createSprite(kScreenWidth, kScreenHeight);
    }
    void dump() {
        if (!ready)
            return;
        Serial.printf("P6\n%d %d\n255\n", kScreenWidth, kScreenHeight);
        uint8_t row[kScreenWidth * 3];
        for (int y = 0; y < kScreenHeight; y++) {
            frame.readRectRGB(0, y, kScreenWidth, 1, row);
            Serial.write(row, sizeof(row));
        }
    }
    void status(bool online) {
        if (!ready)
            return;
        background.fillSprite(black);
        for (int y = 154; y <= 350; y += 16)
            for (int x = 58; x <= 410; x += 16) {
                if ((x - kScreenCenterX) * (x - kScreenCenterX) +
                        (y - kScreenCenterY) * (y - kScreenCenterY) <
                    kGridRadius * kGridRadius)
                    background.drawPixel(x, y, 0x1082);
            }
        // The central divider meets the shared click boundary without a gap.
        for (int x = 0; x < (kScreenWidth - 1); x++)
            background.drawWideLine(x, lroundf(clickBoundary(x)), x + 1,
                                    lroundf(clickBoundary(x + 1)), 0.9f, uint16_t(0x4208));
        background.drawWideLine(kScreenCenterX, 0, kScreenCenterX, kClickBoundaryCenter, 0.9f,
                                uint16_t(0x4208));
        background.setTextDatum(middle_center);
        background.setFont(&fonts::Font2);
        background.setTextColor(muted, black);
        background.fillCircle(178, 385, 3, online ? cyan : muted);
        background.drawString(online ? "CONNECTED" : "PAIR BLUETOOTH", 248, 385);
        update(false, 0, 0, 0, true);
    }
    void update(bool touch, int x, int y, uint8_t buttons, bool force = false) {
        if (!ready)
            return;
        x = constrain(x, 0, (kScreenWidth - 1));
        y = constrain(y, 0, (kScreenHeight - 1));
        if (!force && touch == wasTouch && buttons == oldButtons &&
            (!touch || (x == oldX && y == oldY)))
            return;
        if (!force && millis() - lastFrame < kFrameIntervalMs)
            return;
        lastFrame = millis();
        background.pushSprite(&frame, 0, 0);
        for (int i = 0; i < 2; i++) {
            bool pressed = buttons & (1 << i);
            int cx = i ? 310 : 158;
            if (pressed)
                frame.fillRoundRect(cx - 35, 46, 70, 30, 15, uint16_t(0x1124));
        }
        if (touch) {
            frame.drawCircle(x, y, 18, uint16_t(0x2269));
            frame.drawCircle(x, y, 11, cyan);
            frame.fillCircle(x, y, 3, uint16_t(0xDEFB));
        }
        if (force)
            hardware.flush(frame);
        else {
            if (buttons != oldButtons)
                hardware.flush(frame, 120, 44, 228, 34);
            if (wasTouch)
                hardware.flush(frame, oldX - 21, oldY - 21, 43, 43);
            if (touch)
                hardware.flush(frame, x - 21, y - 21, 43, 43);
        }
        wasTouch = touch;
        oldButtons = buttons;
        oldX = x;
        oldY = y;
    }
};

}  // namespace stopwatch
