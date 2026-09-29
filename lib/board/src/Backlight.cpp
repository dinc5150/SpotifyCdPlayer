#include "Backlight.h"

#include "pins.h"

namespace board {

namespace {
constexpr uint32_t kFrequencyHz = 5000;
constexpr uint8_t kResolutionBits = 10;
constexpr uint32_t kMaxDuty = (1u << kResolutionBits) - 1;
}  // namespace

void Backlight::begin(uint8_t percent) {
  ledcAttach(pins::LCD_BL, kFrequencyHz, kResolutionBits);
  set(percent);
}

void Backlight::set(uint8_t percent) {
  percent_ = percent > 100 ? 100 : percent;
  ledcWrite(pins::LCD_BL, kMaxDuty * percent_ / 100);
}

}  // namespace board
