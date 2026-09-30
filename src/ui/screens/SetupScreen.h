#pragma once

#include "ui/Screen.h"
#include "ui/screens/SetupQr.h"

namespace ui {

// First-time setup (PLAN.md §8.1, Setup 1/3 and 2/3): QR codes to join the
// setup AP, then to open the portal. Step 3 (Link Spotify) comes in Phase 3.
class SetupScreen : public Screen {
 public:
  void build(lv_obj_t *root, const Model &model) override;
  void refresh(const Model &model) override;

 private:
  SetupQr qr_;
};

}  // namespace ui
