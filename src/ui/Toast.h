#pragma once

#include <Arduino.h>

namespace ui {

// A short message at the bottom of the screen for config::kToastMs; a new toast
// replaces the current one. UI task only (post() it from elsewhere).
void showToast(const String &text, bool error = false);

}  // namespace ui
