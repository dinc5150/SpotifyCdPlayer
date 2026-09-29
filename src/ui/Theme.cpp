#include "ui/Theme.h"

#include "config.h"
#include "ui/LvglPort.h"

namespace ui::theme {

namespace {

lv_style_t buttonBase;
lv_style_t buttonPressed;
lv_style_t primary;
lv_style_t danger_;
lv_style_t tile;

void onPressed(lv_event_t *) { port::notePress(); }

}  // namespace

lv_color_t background() { return lv_color_hex(0x0F1115); }
lv_color_t surface() { return lv_color_hex(0x1E2229); }
lv_color_t text() { return lv_color_hex(0xF2F4F7); }
lv_color_t muted() { return lv_color_hex(0x98A2B3); }
lv_color_t accent() { return lv_color_hex(0x5B9BFF); }
lv_color_t danger() { return lv_color_hex(0xF0484E); }

const lv_font_t *fontSmall() { return &lv_font_montserrat_14; }
const lv_font_t *fontBody() { return &lv_font_montserrat_16; }
const lv_font_t *fontTitle() { return &lv_font_montserrat_24; }

void begin(lv_display_t *display) {
  lv_display_set_theme(display, lv_theme_default_init(display, accent(), danger(), true, fontBody()));

  lv_style_init(&buttonBase);
  lv_style_set_bg_color(&buttonBase, surface());
  lv_style_set_bg_opa(&buttonBase, LV_OPA_COVER);
  lv_style_set_text_color(&buttonBase, text());
  lv_style_set_radius(&buttonBase, 10);
  lv_style_set_shadow_width(&buttonBase, 0);  // Shadows cost render time for no gain here
  lv_style_set_pad_all(&buttonBase, 4);

  // Pressed feedback must be obvious at a glance (§9.1): lighter fill plus accent outline.
  lv_style_init(&buttonPressed);
  lv_style_set_bg_color(&buttonPressed, lv_color_lighten(surface(), LV_OPA_30));
  lv_style_set_outline_width(&buttonPressed, 2);
  lv_style_set_outline_color(&buttonPressed, accent());
  lv_style_set_outline_pad(&buttonPressed, 0);

  lv_style_init(&primary);
  lv_style_set_bg_color(&primary, accent());
  lv_style_set_text_color(&primary, lv_color_black());

  lv_style_init(&danger_);
  lv_style_set_bg_color(&danger_, danger());
  lv_style_set_text_color(&danger_, lv_color_white());

  lv_style_init(&tile);
  lv_style_set_radius(&tile, 12);
  lv_style_set_pad_row(&tile, 2);
}

lv_obj_t *createScreen() {
  lv_obj_t *screen = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(screen, background(), 0);
  lv_obj_set_style_pad_all(screen, 0, 0);
  lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  return screen;
}

lv_obj_t *button(lv_obj_t *parent, const char *text, ButtonKind kind, int32_t width, int32_t height) {
  lv_obj_t *btn = lv_button_create(parent);
  lv_obj_set_size(btn, width, height);
  lv_obj_add_style(btn, &buttonBase, 0);
  if (kind == ButtonKind::Primary) lv_obj_add_style(btn, &primary, 0);
  if (kind == ButtonKind::Danger) lv_obj_add_style(btn, &danger_, 0);
  if (kind == ButtonKind::Tile) lv_obj_add_style(btn, &tile, 0);
  lv_obj_add_style(btn, &buttonPressed, LV_STATE_PRESSED);
  if (width < kPrimaryW || height < kPrimaryH) lv_obj_set_ext_click_area(btn, config::kExtClickArea);
  // LVGL keeps a pressed object pressed after the finger slides off it (press
  // lock), so lifting elsewhere still clicks. §9.1: sliding off cancels.
  lv_obj_remove_flag(btn, LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_add_event_cb(btn, onPressed, LV_EVENT_PRESSED, nullptr);

  lv_obj_t *caption = lv_label_create(btn);
  lv_label_set_text(caption, text);
  lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(caption);
  return btn;
}

lv_obj_t *label(lv_obj_t *parent, const char *text, const lv_font_t *font, lv_color_t color) {
  lv_obj_t *l = lv_label_create(parent);
  lv_label_set_text(l, text);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, color, 0);
  return l;
}

lv_obj_t *header(lv_obj_t *parent, const char *title, lv_obj_t **back) {
  lv_obj_t *bar = lv_obj_create(parent);
  lv_obj_remove_style_all(bar);
  lv_obj_set_size(bar, LV_PCT(100), kHeaderH);
  lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

  int32_t titleX = 12;
  if (back) {
    *back = button(bar, LV_SYMBOL_LEFT, ButtonKind::Secondary, kSecondaryW, kHeaderH - 4);
    lv_obj_align(*back, LV_ALIGN_LEFT_MID, 4, 0);
    titleX = 4 + kSecondaryW + 10;
  }
  lv_obj_t *caption = label(bar, title, fontBody(), text());
  lv_obj_align(caption, LV_ALIGN_LEFT_MID, titleX, 0);
  return bar;
}

}  // namespace ui::theme
