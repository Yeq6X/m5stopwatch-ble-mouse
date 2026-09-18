// SPDX-License-Identifier: MIT
#include "../stopwatch_mouse/MouseGestures.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>

using namespace stopwatch;

static TouchFrame frame(std::initializer_list<TouchPoint> points) {
    TouchFrame result;
    for (const auto& point : points)
        result.points[result.count++] = point;
    return result;
}

static void testTapAndHold() {
    MouseGestures mouse;
    auto input = mouse.update(frame({{0, 200, 250}}), 0);
    assert(input.buttons == 0 && input.click == 0);
    assert(mouse.update(frame({{0, 201, 251}}), 100).dx == 0);
    assert(mouse.update(frame({}), 150).click == kLeftButton);
    assert(mouse.update(frame({}), 160).click == 0);
    mouse.reset();
    mouse.update(frame({{0, 200, 250}}), 1000);
    assert(mouse.update(frame({}), 1499).click == kLeftButton);
    mouse.reset();
    mouse.update(frame({{0, 200, 250}}), 1000);
    assert(mouse.update(frame({{0, 200, 250}}), 1500).click == 0);
    assert(mouse.update(frame({}), 1500).click == kRightButton);
    mouse.reset();
    mouse.update(frame({{0, 200, 250}}), 1000);
    assert(mouse.update(frame({{0, 230, 250}}), 1100).dx > 0);
    mouse.update(frame({{0, 200, 250}}), 1200);
    assert(mouse.update(frame({}), 1600).click == 0);  // Returning to origin is still a move.
}

static void testDoubleTapDrag() {
    MouseGestures mouse;
    mouse.update(frame({{0, 200, 250}}), 0);
    assert(mouse.update(frame({}), 80).click == kLeftButton);
    assert(mouse.update(frame({{1, 200, 250}}), 200).buttons == kLeftButton);
    auto input = mouse.update(frame({{1, 230, 250}}), 700);
    assert(input.buttons == kLeftButton && input.dx > 0 && input.click == 0);
    input = mouse.update(frame({}), 800);
    assert(input.buttons == 0 && input.click == 0);

    // A quick second tap produces a second down/up pair without an extra release click.
    mouse.reset();
    mouse.update(frame({{0, 200, 250}}), 0);
    mouse.update(frame({}), 80);
    assert(mouse.update(frame({{0, 200, 250}}), 200).buttons == kLeftButton);
    input = mouse.update(frame({}), 240);
    assert(input.buttons == 0 && input.click == 0);

    mouse.reset();
    mouse.update(frame({{0, 200, 250}}), 0);
    mouse.update(frame({}), 80);
    assert(mouse.update(frame({{0, 200, 250}}), 381).buttons == 0);
    mouse.reset();
    mouse.update(frame({{0, 200, 250}}), 0);
    mouse.update(frame({}), 80);
    assert(mouse.update(frame({{0, 300, 250}}), 200).buttons == 0);
}

static void testButtonDrag(uint8_t button, int buttonX) {
    MouseGestures mouse;
    assert(mouse.update(frame({{3, buttonX, 60}}), 0).buttons == button);
    auto input = mouse.update(frame({{3, buttonX, 60}, {7, 220, 250}}), 100);
    assert(input.buttons == button && input.dx == 0);
    // Reversing record order must preserve the pointer's identity.
    input = mouse.update(frame({{7, 250, 250}, {3, buttonX, 60}}), 150);
    assert(input.buttons == button && input.dx > 0);
    input = mouse.update(frame({{3, buttonX, 60}}), 200);
    assert(input.buttons == button && input.click == 0);
    input = mouse.update(frame({{3, buttonX, 60}, {8, 300, 300}}), 220);
    assert(input.buttons == button && input.dx == 0);  // Lift/reposition during drag.
    input = mouse.update(frame({{8, 310, 300}}), 250);
    assert(input.buttons == 0 && input.click == 0);
    assert(mouse.update(frame({}), 300).click == 0);

    // The pad may be touched before the button, including in the same scan.
    mouse.reset();
    mouse.update(frame({{8, 220, 250}}), 0);
    input = mouse.update(frame({{8, 240, 250}, {3, buttonX, 60}}), 100);
    assert(input.buttons == button && input.dx > 0);
    assert(mouse.update(frame({}), 120).click == 0);
}

static void testSafety() {
    MouseGestures mouse;
    mouse.update(frame({{0, 200, 250}}), 0);
    mouse.update(frame({{0, 200, 50}}), 10);
    auto input = mouse.update(frame({}), 100);
    assert(input.buttons == 0 && input.click == 0);  // Crossing a boundary never clicks.
    mouse.update(frame({{0, 100, 60}}), 200);
    input = mouse.update(frame({{0, 300, 250}}), 250);
    assert(input.buttons == kLeftButton && input.dx == 0);

    TouchFrame invalid;
    invalid.valid = false;
    input = mouse.update(invalid, 300);
    assert(input.buttons == 0 && input.click == 0);
    assert(mouse.update(frame({{0, 200, 250}}), 400).buttons == 0);
    assert(mouse.update(frame({}), 500).click == 0);
    mouse.update(frame({{0, 200, 250}}), 600);
    assert(mouse.update(frame({}), 650).click == kLeftButton);
    mouse.reset();
    assert(mouse.update(frame({}), 700).click == 0);

    // Millisecond wraparound preserves the half-second threshold.
    mouse.reset();
    mouse.update(frame({{0, 200, 250}}), UINT32_MAX - 200);
    assert(mouse.update(frame({}), 299).click == kRightButton);

    // Two pad fingers must never synthesize a release click.
    mouse.reset();
    mouse.update(frame({{0, 200, 250}, {1, 300, 300}}), 0);
    assert(mouse.update(frame({}), 100).click == 0);

    // A changed controller ID without an empty scan is not a new click.
    mouse.reset();
    mouse.update(frame({{0, 200, 250}}), 0);
    input = mouse.update(frame({{1, 202, 251}}), 100);
    assert(input.click == 0 && input.buttons == 0 && input.dx == 0);
    assert(mouse.update(frame({}), 200).click == 0);
}

static void testDecoder() {
    uint8_t bytes[13] = {0, 0, 2, 0x80, 100, 0x30, 60, 0, 0, 0x81, 44, 0x71, 44};
    const auto touch = decodeTouchFrame(bytes, sizeof(bytes));
    assert(touch.valid && touch.count == 2);
    assert(touch.points[0].id == 3 && touch.points[0].x == 100 && touch.points[0].y == 60);
    assert(touch.points[1].id == 7 && touch.points[1].x == 300 && touch.points[1].y == 300);
    bytes[2] = 3;
    assert(!decodeTouchFrame(bytes, sizeof(bytes)).valid);
    bytes[2] = 2;
    bytes[11] = 0x31;
    assert(!decodeTouchFrame(bytes, sizeof(bytes)).valid);  // Duplicate ID.
    assert(!decodeTouchFrame(bytes, 7).valid);
    bytes[2] = 0;
    assert(decodeTouchFrame(bytes, sizeof(bytes)).count == 0);
}

int main() {
    testTapAndHold();
    testDoubleTapDrag();
    testButtonDrag(kLeftButton, 100);
    testButtonDrag(kRightButton, 350);
    testSafety();
    testDecoder();
    std::puts("Gesture and touch decoder tests passed.");
}
