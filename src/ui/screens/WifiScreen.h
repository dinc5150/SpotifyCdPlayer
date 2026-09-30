#pragma once

#include "ui/Screen.h"
#include "ui/screens/SetupQr.h"

namespace ui {

// Menu -> Wi-Fi (PLAN.md §8.2): network, signal, address, and "Set up network",
// which starts the setup AP next to the current connection and shows its QR codes.
class WifiScreen : public Screen {
 public:
  void build(lv_obj_t *root, const Model &model) override;
  void refresh(const Model &model) override;

 private:
  lv_obj_t *info_ = nullptr;     // Details + Set up network
  lv_obj_t *details_ = nullptr;
  lv_obj_t *portal_ = nullptr;   // Setup AP QR codes
  lv_obj_t *stop_ = nullptr;
  SetupQr qr_;
};

}  // namespace ui
