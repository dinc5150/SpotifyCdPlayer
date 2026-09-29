#pragma once

#include <lvgl.h>

// Dark theme, one neutral accent, forgiving touch targets (PLAN.md §9.1).
// Fonts are LVGL's built-in Montserrat (Basic Latin + symbols) until the
// extended Latin/Greek/Cyrillic fonts are generated in Phase 8 (§9.6);
// screens only use the font*() accessors, so the swap is one file.
namespace ui::theme {

enum class ButtonKind { Primary, Secondary, Danger, Tile };

// Target sizes (§9.1): primary >= 72x54, secondary >= 56x44 (with ext click area).
constexpr int32_t kPrimaryW = 72;
constexpr int32_t kPrimaryH = 54;
constexpr int32_t kSecondaryW = 56;
constexpr int32_t kSecondaryH = 44;
constexpr int32_t kGap = 6;
constexpr int32_t kHeaderH = 36;

lv_color_t background();
lv_color_t surface();
lv_color_t text();
lv_color_t muted();
lv_color_t accent();
lv_color_t danger();

const lv_font_t *fontSmall();  // 14 px
const lv_font_t *fontBody();   // 16 px
const lv_font_t *fontTitle();  // 24 px

void begin(lv_display_t *display);

// Plain, non-scrolling screen root with the theme background.
lv_obj_t *createScreen();

// Labelled button. Fires LV_EVENT_CLICKED on release (sliding off cancels).
lv_obj_t *button(lv_obj_t *parent, const char *text, ButtonKind kind, int32_t width, int32_t height);

lv_obj_t *label(lv_obj_t *parent, const char *text, const lv_font_t *font, lv_color_t color);

// Top bar with an optional back button (returned through `back`) and a title.
lv_obj_t *header(lv_obj_t *parent, const char *title, lv_obj_t **back);

}  // namespace ui::theme
