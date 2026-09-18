// SPDX-License-Identifier: MIT
#pragma once

#include <cmath>
#include <cstdint>

namespace stopwatch {
constexpr int kScreenWidth = 468;
constexpr int kScreenHeight = 466;
constexpr int kScreenCenterX = kScreenWidth / 2;
constexpr int kScreenCenterY = kScreenHeight / 2;
constexpr int kPanelOffsetX = 6;
constexpr int kDmaRows = 8;
constexpr int kGridRadius = 195;
constexpr float kClickBoundaryCenter = 88.0f;
constexpr float kClickBoundaryAmplitude = 38.0f;
constexpr float kClickBoundaryPhase = 2.513274f;
constexpr uint32_t kLongPressMs = 300;
constexpr uint32_t kDoubleTapMs = 300;
constexpr int kTapSlop = 8;
constexpr int kDoubleTapSlop = 32;
constexpr uint32_t kClickPulseMs = 24;
constexpr uint32_t kScrollDelayMs = 400;
constexpr uint32_t kScrollRepeatMs = 90;
constexpr uint32_t kButtonDebounceMs = 5;
constexpr uint32_t kFrameIntervalMs = 40;
constexpr uint32_t kBatteryIntervalMs = 60000;
constexpr uint32_t kStatusIntervalMs = 30000;
constexpr uint32_t kPollIntervalMs = 8;
constexpr uint8_t kLeftButton = 1;
constexpr uint8_t kRightButton = 2;
constexpr uint8_t kMaxContacts = 2;

// Drawing and hit testing share exactly the same boundary.
inline float clickBoundary(int x) {
    const float t = std::fabs(float(x - kScreenCenterX)) / kScreenCenterX;
    const float s = std::sin(t * kClickBoundaryPhase);
    return kClickBoundaryCenter + kClickBoundaryAmplitude * s * s;
}
}  // namespace stopwatch
