#include "ui/ConfirmDialog.h"

#include <lvgl.h>

#include "ui/Theme.h"

namespace ui {

namespace {

constexpr int32_t kPanelW = 292;
constexpr int32_t kPanelH = 156;
constexpr int32_t kButtonW = 124;
constexpr int32_t kButtonH = 48;

lv_obj_t *backdrop = nullptr;
app::DialogId openId = app::DialogId::Test;

void close(bool accepted) {
  if (!backdrop) return;
  // Deleted asynchronously: this runs inside a child button's click event.
  lv_obj_delete_async(backdrop);
  backdrop = nullptr;
  app::post(app::EventType::ConfirmResult, static_cast<uint8_t>(openId), accepted ? 1 : 0);
}

}  // namespace

void showConfirm(app::DialogId id, const String &title, const String &message, const String &okText,
                 bool destructive) {
  close(false);
  openId = id;

  // Full-screen dimmed backdrop; it's clickable so taps can't reach the screen below.
  backdrop = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(backdrop);
  lv_obj_set_size(backdrop, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(backdrop, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(backdrop, LV_OPA_60, 0);
  lv_obj_add_flag(backdrop, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *panel = lv_obj_create(backdrop);
  lv_obj_set_size(panel, kPanelW, kPanelH);
  lv_obj_center(panel);
  lv_obj_set_style_bg_color(panel, theme::surface(), 0);
  lv_obj_set_style_border_width(panel, 0, 0);
  lv_obj_set_style_radius(panel, 12, 0);
  lv_obj_set_style_pad_all(panel, 10, 0);
  lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *heading = theme::label(panel, title.c_str(), theme::fontBody(), theme::text());
  lv_obj_align(heading, LV_ALIGN_TOP_LEFT, 0, 0);

  lv_obj_t *body = theme::label(panel, message.c_str(), theme::fontSmall(), theme::muted());
  lv_obj_set_width(body, kPanelW - 20);
  lv_label_set_long_mode(body, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_align(body, LV_ALIGN_TOP_LEFT, 0, 26);

  lv_obj_t *cancel = theme::button(panel, "Cancel", theme::ButtonKind::Secondary, kButtonW, kButtonH);
  lv_obj_align(cancel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_add_event_cb(cancel, [](lv_event_t *) { close(false); }, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *ok = theme::button(panel, okText.c_str(),
                               destructive ? theme::ButtonKind::Danger : theme::ButtonKind::Primary, kButtonW,
                               kButtonH);
  lv_obj_align(ok, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  lv_obj_add_event_cb(ok, [](lv_event_t *) { close(true); }, LV_EVENT_CLICKED, nullptr);
}

}  // namespace ui
