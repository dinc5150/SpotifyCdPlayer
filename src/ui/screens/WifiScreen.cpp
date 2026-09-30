#include "ui/screens/WifiScreen.h"

#include "app/Events.h"
#include "config.h"
#include "net/WifiPolicy.h"
#include "ui/Theme.h"

namespace ui {

namespace {

lv_obj_t *container(lv_obj_t *root) {
  lv_obj_t *c = lv_obj_create(root);
  lv_obj_remove_style_all(c);
  lv_obj_set_size(c, config::kScreenWidth, config::kScreenHeight - theme::kHeaderH);
  lv_obj_set_pos(c, 0, theme::kHeaderH);
  lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  return c;
}

}  // namespace

void WifiScreen::build(lv_obj_t *root, const Model &model) {
  lv_obj_t *back = nullptr;
  lv_obj_t *bar = theme::header(root, "Wi-Fi", &back);
  lv_obj_add_event_cb(back, [](lv_event_t *) { app::post(app::EventType::Back); }, LV_EVENT_CLICKED, nullptr);

  stop_ = theme::button(bar, "Stop setup", theme::ButtonKind::Secondary, 110, theme::kHeaderH - 4);
  lv_obj_align(stop_, LV_ALIGN_RIGHT_MID, -4, 0);
  lv_obj_add_event_cb(stop_, [](lv_event_t *) { app::post(app::EventType::StopSetUpNetwork); }, LV_EVENT_CLICKED,
                      nullptr);

  info_ = container(root);
  details_ = theme::label(info_, "", theme::fontSmall(), theme::text());
  lv_obj_set_style_text_line_space(details_, 4, 0);
  lv_obj_set_width(details_, config::kScreenWidth - 24);
  lv_obj_set_pos(details_, 12, 6);
  lv_obj_t *setup = theme::button(info_, "Set up network", theme::ButtonKind::Secondary, 170, 48);
  lv_obj_align(setup, LV_ALIGN_BOTTOM_RIGHT, -8, -8);
  lv_obj_add_event_cb(setup, [](lv_event_t *) { app::post(app::EventType::SetUpNetwork); }, LV_EVENT_CLICKED,
                      nullptr);

  portal_ = container(root);
  qr_.build(portal_, 8, 4, 172, 128);

  refresh(model);
}

void WifiScreen::refresh(const Model &model) {
  const net::Status &n = model.net;
  const bool portal = n.apActive;
  lv_obj_set_flag(info_, LV_OBJ_FLAG_HIDDEN, portal);
  lv_obj_set_flag(portal_, LV_OBJ_FLAG_HIDDEN, !portal);
  lv_obj_set_flag(stop_, LV_OBJ_FLAG_HIDDEN, !portal);

  if (portal) {
    qr_.refresh(n);
  } else if (n.staConnected) {
    lv_label_set_text_fmt(details_, "Network   %s\nSignal      %s (%ld dBm)\nAddress   %s\nWeb          %s.local",
                          n.ssid.c_str(), wifi_policy::signalWords(n.rssi), static_cast<long>(n.rssi), n.ip.c_str(),
                          n.hostname.c_str());
  } else {
    lv_label_set_text_fmt(details_, "Not connected%s%s\n%u saved network(s)", n.lastError.isEmpty() ? "" : "\n",
                          n.lastError.c_str(), n.savedNetworks);
  }
}

}  // namespace ui
