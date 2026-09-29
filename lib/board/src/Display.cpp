#include "Display.h"

#include "pins.h"

namespace board {

namespace {

// Waveshare ESP32-S3-Touch-LCD-1.47 JD9853 init sequence, from the vendor
// Arduino demo (03_lvgl_arduino_v8, lcd_reg_init). Do not edit.
const uint8_t kJd9853Init[] = {
    BEGIN_WRITE,
    WRITE_COMMAND_8, 0x11,  // Sleep out
    END_WRITE,
    DELAY, 120,

    BEGIN_WRITE,
    WRITE_C8_D16, 0xDF, 0x98, 0x53,
    WRITE_C8_D8, 0xB2, 0x23,

    WRITE_COMMAND_8, 0xB7,
    WRITE_BYTES, 4,
    0x00, 0x47, 0x00, 0x6F,

    WRITE_COMMAND_8, 0xBB,
    WRITE_BYTES, 6,
    0x1C, 0x1A, 0x55, 0x73, 0x63, 0xF0,

    WRITE_C8_D16, 0xC0, 0x44, 0xA4,
    WRITE_C8_D8, 0xC1, 0x16,

    WRITE_COMMAND_8, 0xC3,
    WRITE_BYTES, 8,
    0x7D, 0x07, 0x14, 0x06, 0xCF, 0x71, 0x72, 0x77,

    WRITE_COMMAND_8, 0xC4,
    WRITE_BYTES, 12,
    0x00, 0x00, 0xA0, 0x79, 0x0B, 0x0A, 0x16, 0x79, 0x0B, 0x0A, 0x16, 0x82,

    WRITE_COMMAND_8, 0xC8,
    WRITE_BYTES, 32,
    0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28, 0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,
    0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28, 0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,

    WRITE_COMMAND_8, 0xD0,
    WRITE_BYTES, 5,
    0x04, 0x06, 0x6B, 0x0F, 0x00,

    WRITE_C8_D16, 0xD7, 0x00, 0x30,
    WRITE_C8_D8, 0xE6, 0x14,
    WRITE_C8_D8, 0xDE, 0x01,

    WRITE_COMMAND_8, 0xB7,
    WRITE_BYTES, 5,
    0x03, 0x13, 0xEF, 0x35, 0x35,

    WRITE_COMMAND_8, 0xC1,
    WRITE_BYTES, 3,
    0x14, 0x15, 0xC0,

    WRITE_C8_D16, 0xC2, 0x06, 0x3A,
    WRITE_C8_D16, 0xC4, 0x72, 0x12,
    WRITE_C8_D8, 0xBE, 0x00,
    WRITE_C8_D8, 0xDE, 0x02,

    WRITE_COMMAND_8, 0xE5,
    WRITE_BYTES, 3,
    0x00, 0x02, 0x00,

    WRITE_COMMAND_8, 0xE5,
    WRITE_BYTES, 3,
    0x01, 0x02, 0x00,

    WRITE_C8_D8, 0xDE, 0x00,
    WRITE_C8_D8, 0x35, 0x00,
    WRITE_C8_D8, 0x3A, 0x05,

    WRITE_COMMAND_8, 0x2A,
    WRITE_BYTES, 4,
    0x00, 0x22, 0x00, 0xCD,

    WRITE_COMMAND_8, 0x2B,
    WRITE_BYTES, 4,
    0x00, 0x00, 0x01, 0x3F,

    WRITE_C8_D8, 0xDE, 0x02,

    WRITE_COMMAND_8, 0xE5,
    WRITE_BYTES, 3,
    0x00, 0x02, 0x00,

    WRITE_C8_D8, 0xDE, 0x00,
    WRITE_C8_D8, 0x36, 0x00,
    WRITE_COMMAND_8, 0x21,
    END_WRITE,

    DELAY, 10,

    BEGIN_WRITE,
    WRITE_COMMAND_8, 0x29,  // Display on
    END_WRITE,
};

}  // namespace

bool Display::begin(uint8_t rotation, int32_t spiHz) {
  // Objects are created once. Calling begin() again does not change the SPI clock
  // (the core keeps the bus it already started), so pick the clock at boot.
  if (!bus_) {
    bus_ = new Arduino_ESP32SPI(pins::LCD_DC, pins::LCD_CS, pins::LCD_SCLK, pins::LCD_MOSI,
                                GFX_NOT_DEFINED, FSPI);
    gfx_ = new Arduino_ST7789(bus_, pins::LCD_RST, 0 /* rotation */, false /* IPS */,
                              kPanelWidth, kPanelHeight,
                              34 /* col_offset1 */, 0 /* row_offset1 */,
                              34 /* col_offset2 */, 0 /* row_offset2 */);
  }
  if (!gfx_->begin(spiHz)) return false;
  sendJd9853Init();
  gfx_->setRotation(rotation);
  gfx_->fillScreen(RGB565_BLACK);
  asleep_ = false;
  return true;
}

void Display::setRotation(uint8_t rotation) {
  if (gfx_) gfx_->setRotation(rotation);
}

void Display::sendJd9853Init() { bus_->batchOperation(kJd9853Init, sizeof(kJd9853Init)); }

void Display::sleep() {
  if (!bus_ || asleep_) return;
  bus_->sendCommand(0x28);  // DISPOFF
  bus_->sendCommand(0x10);  // SLPIN
  delay(5);
  asleep_ = true;
}

void Display::wake() {
  if (!bus_ || !asleep_) return;
  bus_->sendCommand(0x11);  // SLPOUT
  delay(120);
  bus_->sendCommand(0x29);  // DISPON
  asleep_ = false;
}

}  // namespace board
