#pragma once

#include <Arduino.h>
#include <Wire.h>

#include <atomic>

// AXS5106L capacitive touch controller (I2C 0x63).
// Based on Waveshare's esp_lcd_touch_axs5106l Arduino driver from the
// ESP32-S3-Touch-LCD-1.47 demo package, reworked: volatile ISR flag, no serial
// output, polling while a finger is down, and a runtime-adjustable transform
// so spike S1 can confirm the landscape mapping on real hardware.
namespace board {

struct TouchPoint {
  uint16_t x = 0;
  uint16_t y = 0;
};

class Touch {
 public:
  // Maps raw controller coordinates (portrait, 172 x 320) to display coordinates.
  struct Transform {
    bool swapXY;
    bool mirrorX;
    bool mirrorY;
  };

  // Rotation 1 (landscape): swap, no mirror (matches the vendor driver's case 1).
  static constexpr Transform kLandscape{true, false, false};
  // Rotation 0 (portrait): mirror X (matches the vendor driver's default case).
  static constexpr Transform kPortrait{false, true, false};

  bool begin(TwoWire &wire, uint16_t displayWidth, uint16_t displayHeight,
             Transform transform = kLandscape);
  void setTransform(Transform transform, uint16_t displayWidth, uint16_t displayHeight);
  Transform transform() const { return transform_; }

  // True while a finger is down. `point` is in display coordinates; `raw`
  // (optional) receives the untransformed controller coordinates. Returns false
  // until begin() has finished, so another task may poll during the ~0.5 s reset.
  bool read(TouchPoint &point, TouchPoint *raw = nullptr);

  // Chip ID bytes read at begin(); all zero means the controller didn't answer.
  const uint8_t *chipId() const { return chipId_; }

  // True if an interrupt fired since the last read(); useful as a wake source.
  static bool interruptPending() { return interrupted_; }

  // esp_timer time (us) of the latest interrupt, for measuring touch-to-pixels latency.
  static int64_t lastInterruptUs() { return lastInterruptUs_; }

 private:
  static void IRAM_ATTR onInterrupt();
  bool readRegister(uint8_t reg, uint8_t *data, size_t length);

  static volatile bool interrupted_;
  static volatile int64_t lastInterruptUs_;
  TwoWire *wire_ = nullptr;
  std::atomic<bool> ready_{false};
  Transform transform_ = kLandscape;
  uint16_t width_ = 320;
  uint16_t height_ = 172;
  bool pressed_ = false;
  uint8_t chipId_[3] = {0, 0, 0};
};

}  // namespace board
