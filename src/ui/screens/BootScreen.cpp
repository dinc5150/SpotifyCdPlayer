#include "ui/screens/BootScreen.h"

#include "ui/Theme.h"

namespace ui {

void BootScreen::build(lv_obj_t *root, const Model &model) {
  lv_obj_t *name = theme::label(root, model.deviceName.c_str(), theme::fontTitle(), theme::text());
  lv_obj_align(name, LV_ALIGN_CENTER, 0, -10);
  lv_obj_t *status = theme::label(root, "Starting...", theme::fontSmall(), theme::muted());
  lv_obj_align(status, LV_ALIGN_CENTER, 0, 20);
}

}  // namespace ui
