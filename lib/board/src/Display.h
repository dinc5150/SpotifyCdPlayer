#pragma once

#include <Arduino_GFX_Library.h>

// JD9853 172x320 IPS panel driven through Arduino_GFX's ST7789 class plus the
// Waveshare JD9853 register init sequence (kept byte for byte from the vendor demo).
namespace board {

class Display {
 public:
  // Landscape by default (rotation 1). Offsets are the panel's 34-column margin
  // in portrait; Arduino_GFX re-applies them for the chosen rotation.
  static constexpr int16_t kPanelWidth = 172;
  static constexpr int16_t kPanelHeight = 320;

  // 80 MHz verified stable on hardware (spike S1, 2026-09-29). Call once: the
  // SPI bus can't be re-clocked cleanly after it starts.
  bool begin(uint8_t rotation = 1, int32_t spiHz = 80000000);
  void setRotation(uint8_t rotation);

  // Panel sleep for the Screen off power profile (PLAN.md §9.5).
  // sleep(): DISPOFF (0x28) + SLPIN (0x10). wake(): SLPOUT (0x11), 120 ms, DISPON (0x29).
  void sleep();
  void wake();
  bool asleep() const { return asleep_; }

  int16_t width() const { return gfx_ ? gfx_->width() : 0; }
  int16_t height() const { return gfx_ ? gfx_->height() : 0; }
  Arduino_GFX *gfx() { return gfx_; }

 private:
  void sendJd9853Init();

  Arduino_DataBus *bus_ = nullptr;
  Arduino_GFX *gfx_ = nullptr;
  bool asleep_ = false;
};

}  // namespace board
