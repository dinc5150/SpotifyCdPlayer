#pragma once

#include <cstdint>

// User-facing settings and the values the UI offers for them (PLAN.md §9.3, §10).
// Pure logic, unit-tested on the host.

// What a tap of the card that's already playing does (§7.2).
enum class SameCard : uint8_t { IgnoreOrResume = 0, Restart = 1, TogglePlayPause = 2 };

struct UiSettings {
  uint8_t brightness = 80;   // percent
  uint16_t dimSeconds = 60;
  uint16_t offSeconds = 300;  // 0 = never
  SameCard sameCard = SameCard::IgnoreOrResume;
  bool pauseOnRemove = false;
  bool albumsInOrder = true;

  bool operator==(const UiSettings &o) const {
    return brightness == o.brightness && dimSeconds == o.dimSeconds && offSeconds == o.offSeconds &&
           sameCard == o.sameCard && pauseOnRemove == o.pauseOnRemove && albumsInOrder == o.albumsInOrder;
  }
  bool operator!=(const UiSettings &o) const { return !(*this == o); }
};

namespace settings_rules {

constexpr uint8_t kMinBrightness = 5;  // 0 would leave the screen unreadably dark
constexpr uint16_t kDimChoices[] = {30, 60, 120, 300};
constexpr uint16_t kOffChoices[] = {120, 300, 600, 0};

// Brings values from NVS or the portal back to what the UI offers: brightness
// is clamped; an unknown timer or same-card value falls back to its default.
// Returns true if anything changed.
bool sanitize(UiSettings &settings);

}  // namespace settings_rules
