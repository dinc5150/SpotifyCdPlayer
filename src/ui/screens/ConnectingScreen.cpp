#include "ui/screens/ConnectingScreen.h"

#include "app/Events.h"
#include "config.h"
#include "ui/Theme.h"

namespace ui {

void ConnectingScreen::build(lv_obj_t *root, const Model &model) {
  theme::header(root, "Connecting to Wi-Fi", nullptr);

  lv_obj_t *spinner = lv_spinner_create(root);
  lv_obj_set_size(spinner, 40, 40);
  lv_obj_set_style_arc_width(spinner, 5, LV_PART_MAIN);
  lv_obj_set_style_arc_width(spinner, 5, LV_PART_INDICATOR);
  lv_obj_align(spinner, LV_ALIGN_TOP_LEFT, 14, 48);

  network_ = theme::label(root, "", theme::fontBody(), theme::text());
  lv_obj_set_width(network_, config::kScreenWidth - 80);
  lv_label_set_long_mode(network_, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
  lv_obj_align(network_, LV_ALIGN_TOP_LEFT, 66, 48);

  error_ = theme::label(root, "", theme::fontSmall(), theme::muted());
  lv_obj_set_width(error_, config::kScreenWidth - 80);
  lv_label_set_long_mode(error_, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_align(error_, LV_ALIGN_TOP_LEFT, 66, 72);

  lv_obj_t *setup = theme::button(root, "Set up network", theme::ButtonKind::Secondary, 180, 48);
  lv_obj_align(setup, LV_ALIGN_BOTTOM_RIGHT, -8, -8);
  lv_obj_add_event_cb(setup, [](lv_event_t *) { app::post(app::EventType::SetUpNetwork); }, LV_EVENT_CLICKED,
                      nullptr);
  refresh(model);
}

void ConnectingScreen::refresh(const Model &model) {
  const net::Status &n = model.net;
  if (n.connecting) {
    lv_label_set_text(network_, n.connectingSsid.c_str());
  } else if (n.savedNetworks == 0) {
    lv_label_set_text(network_, "No network saved");
  } else {
    lv_label_set_text(network_, "Trying again shortly...");
  }
  lv_label_set_text(error_, n.lastError.c_str());
}

}  // namespace ui
