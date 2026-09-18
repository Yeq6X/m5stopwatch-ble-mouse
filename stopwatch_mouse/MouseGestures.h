// SPDX-License-Identifier: MIT
#pragma once

#include "TouchFrame.h"
#include "TouchMouse.h"

namespace stopwatch {
struct MouseInput {
    uint8_t buttons = 0;
    uint8_t click = 0;
    int8_t dx = 0;
    int8_t dy = 0;
    bool pad = false;
    int x = 0;
    int y = 0;
};

// No Arduino/BLE dependencies: contact lifetimes and timing can be tested on a host.
class MouseGestures {
   public:
    void reset() {
        for (auto& contact : contacts_)
            contact = {};
        pointer_.reset();
        pointerId_ = -1;
        tapPending_ = false;
        blocked_ = false;
    }

    MouseInput update(const TouchFrame& frame, uint32_t now) {
        MouseInput out;
        if (!frame.valid) {
            reset();
            blocked_ = true;
            return out;
        }
        if (blocked_) {
            if (!frame.count)
                blocked_ = false;
            return out;
        }
        if (tapPending_ && uint32_t(now - lastTapTime_) > kDoubleTapMs)
            tapPending_ = false;

        // Process releases before reusing slots; IDs survive controller record reordering.
        bool replacedContact = false;
        for (auto& contact : contacts_) {
            if (!contact.active || findPoint(frame, contact.id))
                continue;
            replacedContact = replacedContact || frame.count != 0;
            if (!frame.count && contact.zone == Zone::Pad && !contact.moved && !contact.cancelTap &&
                !contact.doubleHold) {
                out.click =
                    uint32_t(now - contact.started) >= kLongPressMs ? kRightButton : kLeftButton;
                tapPending_ = out.click == kLeftButton;
                lastTapTime_ = now;
                lastTapX_ = contact.startX;
                lastTapY_ = contact.startY;
            }
            contact.active = false;
        }

        for (uint8_t i = 0; i < frame.count; ++i) {
            const auto& point = frame.points[i];
            Contact* contact = findContact(point.id);
            if (!contact) {
                for (auto& slot : contacts_) {
                    if (!slot.active) {
                        contact = &slot;
                        break;
                    }
                }
                if (!contact)
                    continue;
                *contact = {};
                contact->active = true;
                contact->id = point.id;
                contact->zone = point.y < clickBoundary(point.x)
                                    ? (point.x < kScreenCenterX ? Zone::Left : Zone::Right)
                                    : Zone::Pad;
                contact->started = now;
                contact->startX = point.x;
                contact->startY = point.y;
                contact->doubleHold = contact->zone == Zone::Pad && tapPending_ &&
                                      distanceSquared(point.x, point.y, lastTapX_, lastTapY_) <=
                                          kDoubleTapSlop * kDoubleTapSlop;
                tapPending_ = false;
            }
            contact->x = point.x;
            contact->y = point.y;
            if (distanceSquared(point.x, point.y, contact->startX, contact->startY) >
                kTapSlop * kTapSlop)
                contact->moved = true;
        }

        if (frame.count > 1 || replacedContact) {
            // A chord is a drag, never a tap when either finger is lifted.
            out.click = 0;
            tapPending_ = false;
            for (auto& contact : contacts_) {
                contact.cancelTap = true;
                contact.doubleHold = false;
            }
        }

        Contact* pad = nullptr;
        for (auto& contact : contacts_) {
            if (!contact.active)
                continue;
            if (contact.zone == Zone::Left)
                out.buttons |= kLeftButton;
            if (contact.zone == Zone::Right)
                out.buttons |= kRightButton;
            if (contact.zone == Zone::Pad && (!pad || contact.id == pointerId_))
                pad = &contact;
        }
        if (pad) {
            if (pad->doubleHold)
                out.buttons |= kLeftButton;
            if (pointerId_ != pad->id)
                pointer_.reset();
            pointerId_ = pad->id;
            out.pad = true;
            out.x = pad->x;
            out.y = pad->y;
            pointer_.update(true, pad->x, pad->y, out.dx, out.dy);
            // Small stationary-contact jitter must not move a click target.
            if (!pad->moved && !out.buttons)
                out.dx = out.dy = 0;
        } else {
            pointer_.reset();
            pointerId_ = -1;
        }
        return out;
    }

   private:
    enum class Zone { Pad, Left, Right };
    struct Contact {
        bool active = false;
        uint8_t id = 0;
        Zone zone = Zone::Pad;
        uint32_t started = 0;
        int startX = 0, startY = 0, x = 0, y = 0;
        bool moved = false, cancelTap = false, doubleHold = false;
    };
    Contact contacts_[kMaxContacts]{};
    TouchMouse pointer_;
    int pointerId_ = -1;
    bool tapPending_ = false, blocked_ = false;
    uint32_t lastTapTime_ = 0;
    int lastTapX_ = 0, lastTapY_ = 0;

    static int distanceSquared(int x, int y, int otherX, int otherY) {
        const int dx = x - otherX, dy = y - otherY;
        return dx * dx + dy * dy;
    }
    static const TouchPoint* findPoint(const TouchFrame& frame, uint8_t id) {
        for (uint8_t i = 0; i < frame.count; ++i)
            if (frame.points[i].id == id)
                return &frame.points[i];
        return nullptr;
    }
    Contact* findContact(uint8_t id) {
        for (auto& contact : contacts_)
            if (contact.active && contact.id == id)
                return &contact;
        return nullptr;
    }
};
}  // namespace stopwatch
