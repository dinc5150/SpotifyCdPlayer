#include "ui/screens/MenuScreen.h"

#include <cstdint>

#include "app/Events.h"
#include "ui/Theme.h"

namespace ui {

namespace {

constexpr int32_t kTileW = 100;
constexpr int32_t kTileH = 62;

struct Tile {
  const char *text;
  app::Overlay overlay;
};

const Tile kTiles[] = {
    {LV_SYMBOL_AUDIO "\nSpeakers", app::Overlay::Speakers},
    {LV_SYMBOL_EDIT "\nWrite card", app::Overlay::WriteCard},
    {LV_SYMBOL_WIFI "\nWi-Fi", app::Overlay::Wifi},
    {LV_SYMBOL_PLAY "\nSpotify", app::Overlay::Spotify},
    {LV_SYMBOL_SETTINGS "\nSettings", app::Overlay::Settings},
    {LV_SYMBOL_FILE "\nAbout", app::Overlay::About},
};

void onTile(lv_event_t *e) {
  const auto overlay = static_cast<app::Overlay>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
  app::post(app::EventType::OpenOverlay, static_cast<uint8_t>(overlay));
}

}  // namespace

void MenuScreen::build(lv_obj_t *root, const Model &) {
  lv_obj_t *back = nullptr;
  theme::header(root, "Menu", &back);
  lv_obj_add_event_cb(back, [](lv_event_t *) { app::post(app::EventType::Back); }, LV_EVENT_CLICKED, nullptr);

  for (size_t i = 0; i < sizeof(kTiles) / sizeof(kTiles[0]); i++) {
    const int32_t col = i % 3;
    const int32_t row = i / 3;
    lv_obj_t *tile = theme::button(root, kTiles[i].text, theme::ButtonKind::Tile, kTileW, kTileH);
    lv_obj_set_pos(tile, 4 + col * (kTileW + theme::kGap), theme::kHeaderH + 4 + row * (kTileH + theme::kGap));
    lv_obj_add_event_cb(tile, onTile, LV_EVENT_CLICKED,
                        reinterpret_cast<void *>(static_cast<uintptr_t>(kTiles[i].overlay)));
  }
}

}  // namespace ui
