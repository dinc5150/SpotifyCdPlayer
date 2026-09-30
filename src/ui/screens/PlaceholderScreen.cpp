#include "ui/screens/PlaceholderScreen.h"

#include "app/Events.h"
#include "config.h"
#include "ui/Theme.h"

namespace ui {

namespace {

struct Info {
  app::ScreenId id;
  const char *title;
  uint8_t phase;  // PLAN.md §13
  bool overlay;
};

const Info kScreens[] = {
    {app::ScreenId::LinkSpotify, "Link Spotify", 3, false},
    {app::ScreenId::NowPlaying, "Now Playing", 4, false},
    {app::ScreenId::Speakers, "Choose speaker", 6, true},
    {app::ScreenId::WriteCard, "Write a card", 7, true},
    {app::ScreenId::Spotify, "Spotify account", 3, true},
    {app::ScreenId::Settings, "Settings", 8, true},
    {app::ScreenId::About, "About", 8, true},
};

const Info *find(app::ScreenId id) {
  for (const Info &info : kScreens) {
    if (info.id == id) return &info;
  }
  return nullptr;
}

}  // namespace

void PlaceholderScreen::build(lv_obj_t *root, const Model &model) {
  const Info *info = find(id_);
  const bool overlay = info && info->overlay;

  lv_obj_t *back = nullptr;
  theme::header(root, info ? info->title : app::name(id_), overlay ? &back : nullptr);
  if (back) {
    lv_obj_add_event_cb(back, [](lv_event_t *) { app::post(app::EventType::Back); }, LV_EVENT_CLICKED, nullptr);
  }

  lv_obj_t *note = theme::label(root, "", theme::fontBody(), theme::muted());
  if (info) lv_label_set_text_fmt(note, "Arrives in Phase %u", info->phase);
  lv_obj_align(note, LV_ALIGN_CENTER, 0, 0);

  statusLabel_ = theme::label(root, "", theme::fontSmall(), theme::muted());
  lv_obj_set_width(statusLabel_, config::kScreenWidth - 16);
  lv_obj_set_style_text_align(statusLabel_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(statusLabel_, LV_ALIGN_BOTTOM_MID, 0, -10);
  refresh(model);
}

void PlaceholderScreen::refresh(const Model &model) { lv_label_set_text(statusLabel_, model.statusLine.c_str()); }

}  // namespace ui
