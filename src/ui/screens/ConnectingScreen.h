#pragma once

#include "ui/Screen.h"

namespace ui {

// Joining Wi-Fi at boot or after setup, with a way out to the setup AP.
class ConnectingScreen : public Screen {
 public:
  void build(lv_obj_t *root, const Model &model) override;
  void refresh(const Model &model) override;

 private:
  lv_obj_t *network_ = nullptr;
  lv_obj_t *error_ = nullptr;
};

}  // namespace ui
