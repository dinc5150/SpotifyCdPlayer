#include "ui/Toast.h"

#include <lvgl.h>

#include "config.h"
#include "ui/Theme.h"

namespace ui {

namespace {
lv_obj_t *current = nullptr;
}  // namespace

void showToast(const String &text, bool error) {
  if (current) lv_obj_delete(current);

  current = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(current);
  lv_obj_set_size(current, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(current, error ? theme::danger() : lv_color_lighten(theme::surface(), LV_OPA_20), 0);
  lv_obj_set_style_bg_opa(current, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(current, 16, 0);
  lv_obj_set_style_pad_hor(current, 14, 0);
  lv_obj_set_style_pad_ver(current, 8, 0);
  lv_obj_remove_flag(current, LV_OBJ_FLAG_CLICKABLE);  // Taps go through to the screen
  lv_obj_align(current, LV_ALIGN_BOTTOM_MID, 0, -8);

  lv_obj_t *label = theme::label(current, text.c_str(), theme::fontSmall(), theme::text());
  lv_obj_set_style_max_width(label, config::kScreenWidth - 44, 0);
  lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);

  lv_obj_add_event_cb(current, [](lv_event_t *) { current = nullptr; }, LV_EVENT_DELETE, nullptr);
  lv_obj_delete_delayed(current, config::kToastMs);
}

}  // namespace ui
