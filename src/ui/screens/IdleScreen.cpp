#include "ui/screens/IdleScreen.h"

#include "app/Events.h"
#include "config.h"
#include "ui/Theme.h"

namespace ui {

namespace {

constexpr int32_t kChipW = 240;
constexpr int32_t kChipH = 36;
constexpr int32_t kCardW = 64;
constexpr int32_t kCardH = 42;

void setOpacity(void *obj, int32_t value) { lv_obj_set_style_opa(static_cast<lv_obj_t *>(obj), value, 0); }

// Gently pulsing card outline.
void addCardIcon(lv_obj_t *root) {
  lv_obj_t *card = lv_obj_create(root);
  lv_obj_remove_style_all(card);
  lv_obj_set_size(card, kCardW, kCardH);
  lv_obj_set_style_border_width(card, 3, 0);
  lv_obj_set_style_border_color(card, theme::accent(), 0);
  lv_obj_set_style_radius(card, 6, 0);
  lv_obj_align(card, LV_ALIGN_CENTER, 0, -14);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_CLICKABLE);

  lv_anim_t anim;
  lv_anim_init(&anim);
  lv_anim_set_var(&anim, card);
  lv_anim_set_exec_cb(&anim, setOpacity);
  lv_anim_set_values(&anim, LV_OPA_40, LV_OPA_COVER);
  lv_anim_set_duration(&anim, 1200);
  lv_anim_set_reverse_duration(&anim, 1200);
  lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
  lv_anim_start(&anim);  // Removed automatically when the card is deleted
}

}  // namespace

void IdleScreen::build(lv_obj_t *root, const Model &model) {
  lv_obj_t *chip = theme::button(root, "", theme::ButtonKind::Secondary, kChipW, kChipH);
  lv_obj_align(chip, LV_ALIGN_TOP_LEFT, 4, 4);
  speakerLabel_ = lv_obj_get_child(chip, 0);
  lv_obj_set_width(speakerLabel_, kChipW - 16);
  lv_label_set_long_mode(speakerLabel_, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
  lv_obj_add_event_cb(
      chip, [](lv_event_t *) { app::post(app::EventType::OpenOverlay, static_cast<uint8_t>(app::Overlay::Speakers)); },
      LV_EVENT_CLICKED, nullptr);

  lv_obj_t *menu = theme::button(root, LV_SYMBOL_LIST, theme::ButtonKind::Secondary, theme::kSecondaryW, kChipH);
  lv_obj_align(menu, LV_ALIGN_TOP_RIGHT, -4, 4);
  lv_obj_add_event_cb(
      menu, [](lv_event_t *) { app::post(app::EventType::OpenOverlay, static_cast<uint8_t>(app::Overlay::Menu)); },
      LV_EVENT_CLICKED, nullptr);

  addCardIcon(root);

  lv_obj_t *prompt = theme::label(root, "Tap a card to play", theme::fontBody(), theme::text());
  lv_obj_align(prompt, LV_ALIGN_CENTER, 0, 30);

  statusLabel_ = theme::label(root, "", theme::fontSmall(), theme::muted());
  lv_obj_set_width(statusLabel_, config::kScreenWidth - 16);
  lv_obj_set_style_text_align(statusLabel_, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(statusLabel_, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
  lv_obj_align(statusLabel_, LV_ALIGN_BOTTOM_MID, 0, -10);

  refresh(model);
}

void IdleScreen::refresh(const Model &model) {
  const String speaker = model.speakerName.isEmpty() ? String("Choose a speaker") : model.speakerName;
  lv_label_set_text_fmt(speakerLabel_, LV_SYMBOL_AUDIO "  %s  " LV_SYMBOL_DOWN, speaker.c_str());
  lv_label_set_text(statusLabel_, model.statusLine.c_str());
}

}  // namespace ui
