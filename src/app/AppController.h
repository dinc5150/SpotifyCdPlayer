#pragma once

#include "Backlight.h"
#include "app/Events.h"
#include "app/StateMachine.h"
#include "storage/Settings.h"

// The app task: single owner of the app state (PLAN.md §5.1). It reacts to
// AppEvents from the UI, the BOOT button, the Wi-Fi supervisor and the portal,
// and tells the UI what to show by posting ui::Model copies.
namespace app {

void begin(Settings &settings, board::Backlight &backlight);
void startTask();

// Current phase, for other tasks (portal status). A snapshot; may lag by an event.
Phase currentPhase();

}  // namespace app
