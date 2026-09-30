#include "ui/screens/SetupScreen.h"

namespace ui {

void SetupScreen::build(lv_obj_t *root, const Model &model) {
  qr_.build(root, 10, 12, 150, 148);
  refresh(model);
}

void SetupScreen::refresh(const Model &model) { qr_.refresh(model.net); }

}  // namespace ui
