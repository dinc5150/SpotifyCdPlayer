#include "Touch.h"

#include <esp_timer.h>

#include "pins.h"

namespace board {

namespace {
constexpr uint8_t kAddress = 0x63;
constexpr uint8_t kRegChipId = 0x08;
constexpr uint8_t kRegTouchData = 0x01;
constexpr uint32_t kI2cHz = 400000;
}  // namespace

volatile bool Touch::interrupted_ = false;
volatile int64_t Touch::lastInterruptUs_ = 0;

void IRAM_ATTR Touch::onInterrupt() {
  interrupted_ = true;
  lastInterruptUs_ = esp_timer_get_time();
}

bool Touch::begin(TwoWire &wire, uint16_t displayWidth, uint16_t displayHeight,
                  Transform transform) {
  wire_ = &wire;
  wire_->begin(pins::TP_SDA, pins::TP_SCL, kI2cHz);
  setTransform(transform, displayWidth, displayHeight);

  pinMode(pins::TP_RST, OUTPUT);
  digitalWrite(pins::TP_RST, LOW);
  delay(200);
  digitalWrite(pins::TP_RST, HIGH);
  delay(300);

  pinMode(pins::TP_INT, INPUT_PULLUP);
  attachInterrupt(pins::TP_INT, onInterrupt, FALLING);

  const bool ok = readRegister(kRegChipId, chipId_, sizeof(chipId_)) && (chipId_[0] | chipId_[1] | chipId_[2]) != 0;
  ready_ = true;  // read() may now use the bus; it may be called from another task
  return ok;
}

void Touch::setTransform(Transform transform, uint16_t displayWidth, uint16_t displayHeight) {
  transform_ = transform;
  width_ = displayWidth;
  height_ = displayHeight;
}

bool Touch::read(TouchPoint &point, TouchPoint *raw) {
  if (!ready_) return false;
  // Read on a new interrupt, and keep polling while a finger is down so a held
  // press doesn't flicker if the controller only interrupts on changes.
  if (!interrupted_ && !pressed_) return false;
  interrupted_ = false;

  uint8_t data[14] = {0};
  if (!readRegister(kRegTouchData, data, sizeof(data)) || data[1] == 0) {
    pressed_ = false;
    return false;
  }

  TouchPoint r;
  r.x = (static_cast<uint16_t>(data[2] & 0x0F) << 8) | data[3];
  r.y = (static_cast<uint16_t>(data[4] & 0x0F) << 8) | data[5];
  if (raw) *raw = r;

  uint16_t x = transform_.swapXY ? r.y : r.x;
  uint16_t y = transform_.swapXY ? r.x : r.y;
  if (transform_.mirrorX) x = width_ - 1 - min<uint16_t>(x, width_ - 1);
  if (transform_.mirrorY) y = height_ - 1 - min<uint16_t>(y, height_ - 1);
  point.x = min<uint16_t>(x, width_ - 1);
  point.y = min<uint16_t>(y, height_ - 1);
  pressed_ = true;
  return true;
}

bool Touch::readRegister(uint8_t reg, uint8_t *data, size_t length) {
  wire_->beginTransmission(kAddress);
  wire_->write(reg);
  if (wire_->endTransmission() != 0) return false;
  if (wire_->requestFrom(kAddress, length) != length) return false;
  return wire_->readBytes(data, length) == length;
}

}  // namespace board
