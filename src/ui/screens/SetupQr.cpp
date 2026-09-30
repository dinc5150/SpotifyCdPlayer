#include "ui/screens/SetupQr.h"

#include "config.h"
#include "net/WifiPolicy.h"
#include "ui/Theme.h"

namespace ui {

void SetupQr::build(lv_obj_t *parent, int32_t x, int32_t y, int32_t textWidth, int32_t qrSize) {
  step_ = theme::label(parent, "", theme::fontSmall(), theme::accent());
  lv_obj_set_pos(step_, x, y);

  title_ = theme::label(parent, "", theme::fontBody(), theme::text());
  lv_obj_set_width(title_, textWidth);
  lv_label_set_long_mode(title_, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_set_pos(title_, x, y + 20);

  details_ = theme::label(parent, "", theme::fontSmall(), theme::muted());
  lv_obj_set_width(details_, textWidth);
  lv_label_set_long_mode(details_, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_set_pos(details_, x, y + 62);

  // White border = the quiet zone scanners need around the code.
  qr_ = lv_qrcode_create(parent);
  lv_qrcode_set_size(qr_, qrSize - 8);
  lv_qrcode_set_dark_color(qr_, lv_color_black());
  lv_qrcode_set_light_color(qr_, lv_color_white());
  lv_obj_set_style_border_color(qr_, lv_color_white(), 0);
  lv_obj_set_style_border_width(qr_, 4, 0);
  lv_obj_set_pos(qr_, x + textWidth + 8, y);
}

void SetupQr::refresh(const net::Status &net) {
  String qr;
  if (!net.apActive) {
    lv_label_set_text(step_, "");
    lv_label_set_text(title_, "Starting setup Wi-Fi...");
    lv_label_set_text(details_, "");
  } else if (net.apClients == 0) {
    lv_label_set_text(step_, "STEP 1 OF 3");
    lv_label_set_text(title_, "Scan to join the setup Wi-Fi");
    lv_label_set_text_fmt(details_, "Network\n%s\nPassword\n%s", net.apSsid.c_str(), net.apPassword.c_str());
    qr = wifi_policy::wifiQr(net.apSsid.c_str(), net.apPassword.c_str()).c_str();
  } else {
    lv_label_set_text(step_, "STEP 2 OF 3");
    lv_label_set_text(title_, "Scan to open the setup page");
    lv_label_set_text_fmt(details_, "Or browse to\n%s%s%s", config::kPortalIp,
                          net.lastError.isEmpty() ? "" : "\n\n", net.lastError.c_str());
    qr = String("http://") + config::kPortalIp + "/setup";
  }
  lv_obj_add_flag(qr_, LV_OBJ_FLAG_HIDDEN);
  if (!qr.isEmpty()) {
    lv_obj_remove_flag(qr_, LV_OBJ_FLAG_HIDDEN);
    if (qr != qrText_) lv_qrcode_update(qr_, qr.c_str(), qr.length());
  }
  qrText_ = qr;
}

}  // namespace ui
