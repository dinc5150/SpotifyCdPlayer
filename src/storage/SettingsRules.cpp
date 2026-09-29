#include "storage/SettingsRules.h"

#include <cstddef>

namespace settings_rules {

namespace {

template <size_t N>
bool contains(const uint16_t (&choices)[N], uint16_t value) {
  for (uint16_t choice : choices) {
    if (choice == value) return true;
  }
  return false;
}

}  // namespace

bool sanitize(UiSettings &s) {
  const UiSettings defaults;
  const UiSettings before = s;

  if (s.brightness < kMinBrightness) s.brightness = kMinBrightness;
  if (s.brightness > 100) s.brightness = 100;
  if (!contains(kDimChoices, s.dimSeconds)) s.dimSeconds = defaults.dimSeconds;
  if (!contains(kOffChoices, s.offSeconds)) s.offSeconds = defaults.offSeconds;
  if (static_cast<uint8_t>(s.sameCard) > static_cast<uint8_t>(SameCard::TogglePlayPause)) {
    s.sameCard = defaults.sameCard;
  }
  return s != before;
}

}  // namespace settings_rules
