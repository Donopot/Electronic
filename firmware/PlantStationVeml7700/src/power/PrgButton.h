#pragma once
#include <stdint.h>

namespace plant {
enum class ButtonEvent { None, ShortPress, LongPress, ReleasedForSleep, SetupHold, ReleasedForSetup };

class PrgButton {
public:
    void begin(uint32_t now, bool pressed) {
        raw_ = pressed_ = pressed;
        changed_ = pressedAt_ = now;
        armed_ = !pressed;
        requested_ = false;
        setupRequested_ = false;
    }
    ButtonEvent update(uint32_t now, bool pressed, bool reportShortPress = false) {
        if (pressed != raw_) { raw_ = pressed; changed_ = now; }
        const bool stable = static_cast<uint32_t>(now - changed_) >= 40;
        if (stable && pressed_ != raw_) {
            pressed_ = raw_;
            if (pressed_) pressedAt_ = changed_;
            else if (setupRequested_) {setupRequested_=requested_=false;return ButtonEvent::ReleasedForSetup;}
            else if (requested_) return ButtonEvent::ReleasedForSleep;
            else if (armed_ && reportShortPress && static_cast<uint32_t>(changed_ - pressedAt_) < 3000)
                return ButtonEvent::ShortPress;
        }
        if (!pressed_) {
            if (stable && !raw_) armed_ = true;
            return ButtonEvent::None;
        }
        // Geler la duree au debut du relachement, pas a sa fin de debounce.
        const uint32_t heldUntil = raw_ ? now : changed_;
        if(armed_ && reportShortPress && !setupRequested_ && static_cast<uint32_t>(heldUntil-pressedAt_)>=8000) {
            setupRequested_=requested_=true;
            return ButtonEvent::SetupHold;
        }
        if (armed_ && !requested_ && static_cast<uint32_t>(heldUntil - pressedAt_) >= 3000) {
            requested_ = true;
            return ButtonEvent::LongPress;
        }
        return ButtonEvent::None;
    }
private:
    bool raw_ = false, pressed_ = false, armed_ = false, requested_ = false, setupRequested_ = false;
    uint32_t changed_ = 0, pressedAt_ = 0;
};
}
