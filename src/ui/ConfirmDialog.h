#pragma once

#include <Arduino.h>

#include "app/Events.h"

namespace ui {

// Modal confirmation for destructive actions (§9.1). The answer goes back to
// the app as EventType::ConfirmResult with the dialog id. Opening another
// dialog cancels the current one. UI task only.
void showConfirm(app::DialogId id, const String &title, const String &message, const String &okText,
                 bool destructive);

}  // namespace ui
