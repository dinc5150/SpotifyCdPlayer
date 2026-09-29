#pragma once

#include <cstdint>

// Debounces a push button and turns it into the gestures in PLAN.md §9.4.
// Pure logic, unit-tested on the host; hal::BootButton feeds it GPIO0.

enum class ButtonEvent : uint8_t {
  None,
  ShortPress,   // released before the long-press time
  LongPress,    // fires once while held, at longPressMs
  FactoryHold,  // fires once while held, at factoryHoldMs (after LongPress)
};

class ButtonGesture {
 public:
  ButtonGesture(uint32_t debounceMs, uint32_t longPressMs, uint32_t factoryHoldMs);

  // Call every few ms with the raw level (true = pressed).
  ButtonEvent update(bool rawPressed, uint32_t nowMs);

  bool pressed() const { return stable_; }

 private:
  uint32_t debounceMs_;
  uint32_t longPressMs_;
  uint32_t factoryHoldMs_;

  bool raw_ = false;
  bool stable_ = false;
  uint32_t rawChangedMs_ = 0;
  uint32_t pressedAtMs_ = 0;
  bool longFired_ = false;
  bool factoryFired_ = false;
};
