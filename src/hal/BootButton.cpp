#include "hal/BootButton.h"

#include <Arduino.h>

#include "config.h"
#include "pins.h"

namespace hal {

BootButton::BootButton()
    : gesture_(config::kButtonDebounceMs, config::kButtonLongPressMs, config::kButtonFactoryHoldMs) {}

void BootButton::begin() { pinMode(pins::BOOT_BUTTON, INPUT_PULLUP); }

ButtonEvent BootButton::poll() { return gesture_.update(digitalRead(pins::BOOT_BUTTON) == LOW, millis()); }

}  // namespace hal
