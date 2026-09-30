#pragma once

#include <lvgl.h>

#include "net/WifiSupervisor.h"

namespace ui {

// The setup AP's two QR steps (PLAN.md §8.1), shared by the Setup screen and
// the Wi-Fi overlay: until a phone joins, a Wi-Fi QR (SSID + password); after,
// a QR for the setup page. A text column beside the QR says the same in words.
class SetupQr {
 public:
  // `textWidth` is the column left of the QR; the QR is `qrSize` px square.
  void build(lv_obj_t *parent, int32_t x, int32_t y, int32_t textWidth, int32_t qrSize);
  void refresh(const net::Status &net);

 private:
  lv_obj_t *step_ = nullptr;
  lv_obj_t *title_ = nullptr;
  lv_obj_t *details_ = nullptr;
  lv_obj_t *qr_ = nullptr;
  String qrText_;
};

}  // namespace ui
