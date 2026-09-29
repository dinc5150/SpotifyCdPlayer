#include "util/ButtonGesture.h"

ButtonGesture::ButtonGesture(uint32_t debounceMs, uint32_t longPressMs, uint32_t factoryHoldMs)
    : debounceMs_(debounceMs), longPressMs_(longPressMs), factoryHoldMs_(factoryHoldMs) {}

ButtonEvent ButtonGesture::update(bool rawPressed, uint32_t nowMs) {
  if (rawPressed != raw_) {
    raw_ = rawPressed;
    rawChangedMs_ = nowMs;
  }

  // A level counts once it has been steady for the debounce time.
  if (raw_ != stable_ && nowMs - rawChangedMs_ >= debounceMs_) {
    stable_ = raw_;
    if (stable_) {
      pressedAtMs_ = rawChangedMs_;
      longFired_ = false;
      factoryFired_ = false;
    } else if (!longFired_) {
      return ButtonEvent::ShortPress;
    }
  }

  if (stable_) {
    const uint32_t held = nowMs - pressedAtMs_;
    if (!longFired_ && held >= longPressMs_) {
      longFired_ = true;
      return ButtonEvent::LongPress;
    }
    if (!factoryFired_ && held >= factoryHoldMs_) {
      factoryFired_ = true;
      return ButtonEvent::FactoryHold;
    }
  }
  return ButtonEvent::None;
}
