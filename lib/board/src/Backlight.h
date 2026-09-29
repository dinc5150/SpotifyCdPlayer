#pragma once

#include <Arduino.h>

namespace board {

// LCD backlight on GPIO46 via LEDC PWM (5 kHz, 10-bit, as in the vendor demo).
class Backlight {
 public:
  void begin(uint8_t percent = 80);
  void set(uint8_t percent);
  uint8_t get() const { return percent_; }

 private:
  uint8_t percent_ = 0;
};

}  // namespace board
